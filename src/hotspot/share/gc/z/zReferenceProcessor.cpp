/*
 * Copyright (c) 2015, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "precompiled.hpp"
#include "classfile/javaClasses.inline.hpp"
#include "gc/shared/referencePolicy.hpp"
#include "gc/shared/referenceProcessorStats.hpp"
#include "gc/shared/suspendibleThreadSet.hpp"
#include "gc/z/zCollectedHeap.hpp"
#include "gc/z/zDriver.hpp"
#include "gc/z/zHeap.inline.hpp"
#include "gc/z/zReferenceProcessor.hpp"
#include "gc/z/zStat.hpp"
#include "gc/z/zTask.hpp"
#include "gc/z/zTracer.inline.hpp"
#include "gc/z/zValue.inline.hpp"
#include "memory/universe.hpp"
#include "oops/access.inline.hpp"
#include "runtime/atomic.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/os.hpp"

static const ZStatSubPhase ZSubPhaseConcurrentReferencesProcess("Old: Concurrent References Process");
static const ZStatSubPhase ZSubPhaseConcurrentReferencesEnqueue("Old: Concurrent References Enqueue");

static ReferenceType reference_type(zaddress reference) {
  return InstanceKlass::cast(to_oop(reference)->klass())->reference_type();
}

static const char* reference_type_name(ReferenceType type) {
  switch (type) {
  case REF_SOFT:
    return "Soft";

  case REF_WEAK:
    return "Weak";

  case REF_FINAL:
    return "Final";

  case REF_PHANTOM:
    return "Phantom";

  default:
    ShouldNotReachHere();
    return NULL;
  }
}

static volatile zpointer* reference_referent_addr(zaddress reference) {
  return (volatile zpointer*)java_lang_ref_Reference::referent_addr_raw(to_oop(reference));
}

static zpointer reference_referent(zaddress reference) {
  return ZBarrier::load_atomic(reference_referent_addr(reference));
}

static zpointer* reference_discovered_addr(zaddress reference) {
  return (zpointer*)java_lang_ref_Reference::discovered_addr_raw(to_oop(reference));
}

static zaddress reference_discovered(zaddress reference) {
  return to_zaddress(java_lang_ref_Reference::discovered(to_oop(reference)));
}

static void reference_set_discovered(zaddress reference, zaddress discovered) {
  java_lang_ref_Reference::set_discovered(to_oop(reference), to_oop(discovered));
}

static zaddress reference_next(zaddress reference) {
  return to_zaddress(java_lang_ref_Reference::next(to_oop(reference)));
}

static void reference_set_next(zaddress reference, zaddress next) {
  java_lang_ref_Reference::set_next(to_oop(reference), to_oop(next));
}

static void soft_reference_update_clock() {
  SuspendibleThreadSetJoiner sts_joiner;
  const jlong now = os::javaTimeNanos() / NANOSECS_PER_MILLISEC;
  java_lang_ref_SoftReference::set_clock(now);
}

ZReferenceProcessor::ZReferenceProcessor(ZWorkers* workers) :
    _workers(workers),
    _soft_reference_policy(NULL),
    _encountered_count(),
    _discovered_count(),
    _enqueued_count(),
    _discovered_list(zpointer::null),
    _pending_list(zpointer::null),
    _pending_list_tail(_pending_list.addr()) {}

void ZReferenceProcessor::set_soft_reference_policy(bool clear) {
  static AlwaysClearPolicy always_clear_policy;
  static LRUMaxHeapPolicy lru_max_heap_policy;

  if (clear) {
    log_info(gc, ref)("Clearing All SoftReferences");
    _soft_reference_policy = &always_clear_policy;
  } else {
    _soft_reference_policy = &lru_max_heap_policy;
  }

  _soft_reference_policy->setup();
}

bool ZReferenceProcessor::is_inactive(zaddress reference, oop referent, ReferenceType type) const {
  if (type == REF_FINAL) {
    // A FinalReference is inactive if its next field is non-null. An application can't
    // call enqueue() or clear() on a FinalReference.
    return !is_null(reference_next(reference));
  } else {
    // A non-FinalReference is inactive if the referent is null. The referent can only
    // be null if the application called Reference.enqueue() or Reference.clear().
    (void)to_zaddress(referent); // Verifications
    return referent == NULL;
  }
}

bool ZReferenceProcessor::is_strongly_live(oop referent) const {
  const zaddress addr = to_zaddress(referent);
  return ZHeap::heap()->is_young(addr) || ZHeap::heap()->is_object_strongly_live(to_zaddress(referent));
}

bool ZReferenceProcessor::is_softly_live(zaddress reference, ReferenceType type) const {
  if (type != REF_SOFT) {
    // Not a SoftReference
    return false;
  }

  // Ask SoftReference policy
  const jlong clock = java_lang_ref_SoftReference::clock();
  assert(clock != 0, "Clock not initialized");
  assert(_soft_reference_policy != NULL, "Policy not initialized");
  return !_soft_reference_policy->should_clear_reference(to_oop(reference), clock);
}

bool ZReferenceProcessor::should_discover(zaddress reference, ReferenceType type) const {
  volatile zpointer* const referent_addr = reference_referent_addr(reference);
  const oop referent = to_oop(ZBarrier::load_barrier_on_oop_field(referent_addr));

  if (is_inactive(reference, referent, type)) {
    return false;
  }

  if (ZHeap::heap()->is_young(reference)) {
    return false;
  }

  if (is_strongly_live(referent)) {
    return false;
  }

  if (is_softly_live(reference, type)) {
    return false;
  }

  // PhantomReferences with finalizable marked referents should technically not have
  // to be discovered. However, InstanceRefKlass::oop_oop_iterate_ref_processing()
  // does not know about the finalizable mark concept, and will therefore mark
  // referents in non-discovered PhantomReferences as strongly live. To prevent
  // this, we always discover PhantomReferences with finalizable marked referents.
  // They will automatically be dropped during the reference processing phase.
  return true;
}

bool ZReferenceProcessor::try_make_inactive(zaddress reference, ReferenceType type) const {
  const zpointer referent = reference_referent(reference);

  if (is_null_any(referent)) {
    // Reference has already been cleared, by a call to Reference.enqueue()
    // or Reference.clear() from the application, which means it's already
    // inactive and we should drop the reference.
    return false;
  }

  volatile zpointer* const referent_addr = reference_referent_addr(reference);

  // Cleaning the referent will fail if the object it points to is
  // still alive, in which case we should drop the reference.
  if (type == REF_SOFT || type == REF_WEAK) {
    return ZBarrier::clean_barrier_on_weak_oop_field(referent_addr);
  } else if (type == REF_PHANTOM) {
    return ZBarrier::clean_barrier_on_phantom_oop_field(referent_addr);
  } else if (type == REF_FINAL) {
    if (ZBarrier::clean_barrier_on_final_oop_field(referent_addr)) {
      // The referent in a FinalReference will not be cleared, instead it is
      // made inactive by self-looping the next field. An application can't
      // call FinalReference.enqueue(), so there is no race to worry about
      // when setting the next field.
      assert(is_null(reference_next(reference)), "Already inactive");
      reference_set_next(reference, reference);
      return true;
    }
  } else {
    fatal("Invalid referent type %d", type);
  }

  return false;
}

void ZReferenceProcessor::discover(zaddress reference, ReferenceType type) {
  log_trace(gc, ref)("Discovered Reference: " PTR_FORMAT " (%s)", untype(reference), reference_type_name(type));

  // Update statistics
  _discovered_count.get()[type]++;

  if (type == REF_FINAL) {
    // Mark referent (and its reachable subgraph) finalizable. This avoids
    // the problem of later having to mark those objects if the referent is
    // still final reachable during processing.
    volatile zpointer* const referent_addr = reference_referent_addr(reference);
    ZBarrier::mark_barrier_on_oop_field(referent_addr, true /* finalizable */);
  }

  // Add reference to discovered list
  assert(ZHeap::heap()->is_old(reference), "Must be old");
  assert(is_null(reference_discovered(reference)), "Already discovered");
  zpointer* const list = _discovered_list.addr();
  reference_set_discovered(reference, ZBarrier::load_barrier_on_oop_field(list));
  *list = ZAddress::store_good(reference);
}

bool ZReferenceProcessor::discover_reference(oop reference_obj, ReferenceType type) {
  if (!RegisterReferences) {
    // Reference processing disabled
    return false;
  }

  log_trace(gc, ref)("Encountered Reference: " PTR_FORMAT " (%s)", p2i(reference_obj), reference_type_name(type));

  zaddress reference = to_zaddress(reference_obj);

  // Update statistics
  _encountered_count.get()[type]++;

  if (!should_discover(reference, type)) {
    // Not discovered
    return false;
  }

  discover(reference, type);

  // Discovered
  return true;
}

zpointer ZReferenceProcessor::drop(zaddress reference, ReferenceType type) {
  log_trace(gc, ref)("Dropped Reference: " PTR_FORMAT " (%s)", untype(reference), reference_type_name(type));

  // Unlink and return next in list
  const zaddress next = reference_discovered(reference);
  reference_set_discovered(reference, zaddress::null);
  assert(*reference_discovered_addr(reference) != zpointer::null, "No raw nulls");
  return ZAddress::store_good(next);
}

zpointer* ZReferenceProcessor::keep(zaddress reference, ReferenceType type) {
  log_trace(gc, ref)("Enqueued Reference: " PTR_FORMAT " (%s)", untype(reference), reference_type_name(type));

  // Update statistics
  _enqueued_count.get()[type]++;

  // Return next in list
  return reference_discovered_addr(reference);
}

void ZReferenceProcessor::process_worker_discovered_list(zpointer discovered_list) {
  zpointer* const start = &discovered_list;

  // The list is chained through the discovered field,
  // but the first entry is not in the heap.
  auto store_in_list = [&](zpointer* p, zpointer ptr) {
    if (p != start) {
      ZBarrier::store_barrier_on_heap_oop_field(p, false /* heal */);
    }
    *p = ptr;
  };

  zpointer* p = start;

  for (;;) {
    const zaddress reference = ZBarrier::load_barrier_on_oop_field(p);

    if (is_null(reference)) {
      // End of list
      break;
    }

    const ReferenceType type = reference_type(reference);

    if (try_make_inactive(reference, type)) {
      p = keep(reference, type);
    } else {
      store_in_list(p, drop(reference, type));
      assert(*p != zpointer::null, "No NULL");
    }

    SuspendibleThreadSet::yield();
  }

  zpointer* end = p;

  // Prepend discovered references to internal pending list

  // Anything keept on the list?
  if (!is_null_any(*start)) {
    zpointer old_pending_list = Atomic::xchg(_pending_list.addr(), *start);

    // Old list could be empty and contain a raw null, don't store raw nulls.
    if (old_pending_list == zpointer::null) {
      old_pending_list = ZAddress::store_good(zaddress::null);
    }

    // Concatenate the old list
    store_in_list(end,  old_pending_list);

    if (is_null_any(old_pending_list)) {
      // Old list was empty. First to prepend to list, record tail
      _pending_list_tail = end;
    }
  }
}

void ZReferenceProcessor::work() {
  SuspendibleThreadSetJoiner sts;

  ZPerWorkerIterator<zpointer> iter(&_discovered_list);
  for (zpointer* start; iter.next(&start);) {
    zpointer discovered_list = Atomic::xchg(start, zpointer::null);

    if (discovered_list != zpointer::null) {
      // Process discovered references
      process_worker_discovered_list(discovered_list);
    }
  }
}

void ZReferenceProcessor::verify_empty() const {
#ifdef ASSERT
  ZPerWorkerConstIterator<zpointer> iter(&_discovered_list);
  for (const zpointer* list; iter.next(&list);) {
    assert(is_null(*list), "Discovered list not empty");
  }

  assert(is_null(_pending_list.get()), "Pending list not empty");
#endif
}

void ZReferenceProcessor::reset_statistics() {
  verify_empty();

  // Reset encountered
  ZPerWorkerIterator<Counters> iter_encountered(&_encountered_count);
  for (Counters* counters; iter_encountered.next(&counters);) {
    for (int i = REF_SOFT; i <= REF_PHANTOM; i++) {
      (*counters)[i] = 0;
    }
  }

  // Reset discovered
  ZPerWorkerIterator<Counters> iter_discovered(&_discovered_count);
  for (Counters* counters; iter_discovered.next(&counters);) {
    for (int i = REF_SOFT; i <= REF_PHANTOM; i++) {
      (*counters)[i] = 0;
    }
  }

  // Reset enqueued
  ZPerWorkerIterator<Counters> iter_enqueued(&_enqueued_count);
  for (Counters* counters; iter_enqueued.next(&counters);) {
    for (int i = REF_SOFT; i <= REF_PHANTOM; i++) {
      (*counters)[i] = 0;
    }
  }
}

void ZReferenceProcessor::collect_statistics() {
  Counters encountered = {};
  Counters discovered = {};
  Counters enqueued = {};

  // Sum encountered
  ZPerWorkerConstIterator<Counters> iter_encountered(&_encountered_count);
  for (const Counters* counters; iter_encountered.next(&counters);) {
    for (int i = REF_SOFT; i <= REF_PHANTOM; i++) {
      encountered[i] += (*counters)[i];
    }
  }

  // Sum discovered
  ZPerWorkerConstIterator<Counters> iter_discovered(&_discovered_count);
  for (const Counters* counters; iter_discovered.next(&counters);) {
    for (int i = REF_SOFT; i <= REF_PHANTOM; i++) {
      discovered[i] += (*counters)[i];
    }
  }

  // Sum enqueued
  ZPerWorkerConstIterator<Counters> iter_enqueued(&_enqueued_count);
  for (const Counters* counters; iter_enqueued.next(&counters);) {
    for (int i = REF_SOFT; i <= REF_PHANTOM; i++) {
      enqueued[i] += (*counters)[i];
    }
  }

  // Update statistics
  ZStatReferences::set_soft(encountered[REF_SOFT], discovered[REF_SOFT], enqueued[REF_SOFT]);
  ZStatReferences::set_weak(encountered[REF_WEAK], discovered[REF_WEAK], enqueued[REF_WEAK]);
  ZStatReferences::set_final(encountered[REF_FINAL], discovered[REF_FINAL], enqueued[REF_FINAL]);
  ZStatReferences::set_phantom(encountered[REF_PHANTOM], discovered[REF_PHANTOM], enqueued[REF_PHANTOM]);

  // Trace statistics
  const ReferenceProcessorStats stats(discovered[REF_SOFT],
                                      discovered[REF_WEAK],
                                      discovered[REF_FINAL],
                                      discovered[REF_PHANTOM]);
  ZDriver::major()->jfr_tracer()->report_gc_reference_stats(stats);
}

class ZReferenceProcessorTask : public ZTask {
private:
  ZReferenceProcessor* const _reference_processor;

public:
  ZReferenceProcessorTask(ZReferenceProcessor* reference_processor) :
      ZTask("ZReferenceProcessorTask"),
      _reference_processor(reference_processor) {}

  virtual void work() {
    _reference_processor->work();
  }
};

void ZReferenceProcessor::process_references() {
  ZStatTimerOld timer(ZSubPhaseConcurrentReferencesProcess);

  // Process discovered lists
  ZReferenceProcessorTask task(this);
  _workers->run(&task);

  // Update SoftReference clock
  soft_reference_update_clock();

  // Collect, log and trace statistics
  collect_statistics();
}

void ZReferenceProcessor::verify_pending_references() {
#ifdef ASSERT
  SuspendibleThreadSetJoiner sts;

  assert(!is_null_any(_pending_list.get()), "Should not contain colored null");

  for (zaddress current = ZBarrier::load_barrier_on_oop_field_preloaded(NULL, _pending_list.get());
       !is_null(current);
       current = reference_discovered(current))
  {
    volatile zpointer* const referent_addr = reference_referent_addr(current);
    const oop referent = to_oop(ZBarrier::load_barrier_on_oop_field(referent_addr));
    const ReferenceType type = reference_type(current);
    assert(ZReferenceProcessor::is_inactive(current, referent, type), "invariant");
    if (type == REF_FINAL) {
      assert(ZPointer::is_marked_any_old(ZBarrier::load_atomic(referent_addr)), "invariant");
    }

    SuspendibleThreadSet::yield();
  }
#endif
}

zpointer ZReferenceProcessor::swap_pending_list(zpointer pending_list) {
  oop pending_list_oop = to_oop(ZBarrier::load_barrier_on_oop_field_preloaded(NULL, pending_list));
  oop prev = Universe::swap_reference_pending_list(pending_list_oop);
  return ZAddress::store_good(to_zaddress(prev));
}

void ZReferenceProcessor::enqueue_references() {
  ZStatTimerOld timer(ZSubPhaseConcurrentReferencesEnqueue);

  if (is_null(_pending_list.get())) {
    // Nothing to enqueue
    return;
  }

  // Verify references on internal pending list
  verify_pending_references();

  {
    // Heap_lock protects external pending list
    MonitorLocker ml(Heap_lock);
    SuspendibleThreadSetJoiner sts_joiner;

    // Prepend internal pending list to external pending list
    *_pending_list_tail = swap_pending_list(_pending_list.get());

    // Notify ReferenceHandler thread
    ml.notify_all();
  }

  // Reset internal pending list
  _pending_list.set(zpointer::null);
  _pending_list_tail = _pending_list.addr();
}

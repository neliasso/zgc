/*
 * Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
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
#include "gc/z/zGeneration.inline.hpp"
#include "gc/z/zList.inline.hpp"
#include "gc/z/zPage.inline.hpp"
#include "gc/z/zPhysicalMemory.inline.hpp"
#include "gc/z/zRememberedSet.inline.hpp"
#include "gc/z/zVirtualMemory.inline.hpp"
#include "logging/logStream.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"
#include "utilities/growableArray.hpp"

ZPage::ZPage(ZPageType type, const ZVirtualMemory& vmem, const ZPhysicalMemory& pmem) :
    _type(type),
    _generation_id(ZGenerationId::young),
    _age(ZPageAge::eden),
    _numa_id((uint8_t)-1),
    _seqnum(0),
    _seqnum_other(0),
    _virtual(vmem),
    _top(start()),
    _livemap(object_max_count()),
    _remembered_set(),
    _last_used(0),
    _physical(pmem),
    _node() {
  assert(!_virtual.is_null(), "Should not be null");
  assert(!_physical.is_null(), "Should not be null");
  assert(_virtual.size() == _physical.size(), "Virtual/Physical size mismatch");
  assert((_type == ZPageType::small && size() == ZPageSizeSmall) ||
         (_type == ZPageType::medium && size() == ZPageSizeMedium) ||
         (_type == ZPageType::large && is_aligned(size(), ZGranuleSize)),
         "Page type/size mismatch");
}

ZPage::~ZPage() {}

ZPage* ZPage::clone_limited() const {
  // Only copy type and memory layouts. Let the rest be lazily reconstructed when needed.
  return new ZPage(_type, _virtual, _physical);
}

ZPage* ZPage::clone_limited_promote_flipped() const {
  ZPage* page = new ZPage(_type, _virtual, _physical);

  // The page is still filled with the same objects, need to retain the top pointer.
  page->_top = _top;

  return page;
}

ZGeneration* ZPage::generation() {
  return ZGeneration::generation(_generation_id);
}

const ZGeneration* ZPage::generation() const {
  return ZGeneration::generation(_generation_id);
}

void ZPage::reset_seqnum() {
  Atomic::store(&_seqnum, generation()->seqnum());
  Atomic::store(&_seqnum_other, ZGeneration::generation(_generation_id == ZGenerationId::young ? ZGenerationId::old : ZGenerationId::young)->seqnum());
}

void ZPage::reset_remembered_set(ZPageAge prev_age, ZPageResetType type) {
  if (is_young()) {
    // Remset not needed
    return;
  }

  // Young-to-old reset
  if (prev_age != ZPageAge::old) {
    if (!_remembered_set.is_initialized()) {
      _remembered_set.initialize(size());
      return;
    }

    // We don't clear the remset when pages are recycled and transition from
    // old to young. Therefore we can end up in a situation where a young page
    // already has an initialized remset.
    _remembered_set.clear();
    return;
  }

  // Old-to-old reset
  switch (type) {
  case ZPageResetType::Splitting:
    // Page is on the way to be destroyed or reused, delay
    // clearing until the page is reset for Allocation.
    break;

  case ZPageResetType::InPlaceRelocation:
    // Relocation failed and page is being compacted in-place. Current bits
    // are needed to copy the remset incrementally. It will get cleared later on.
    if (ZGeneration::young()->is_phase_mark()) {
      _remembered_set.clear_current();
    } else {
      _remembered_set.clear_previous();
    }
    break;

  case ZPageResetType::FlipAging:
    // Page stayed in the old gen. Needs new, fresh bits.
    _remembered_set.clear();
    break;

  case ZPageResetType::Allocation:
    _remembered_set.clear();
    break;
  };
}

void ZPage::reset(ZPageAge age, ZPageResetType type) {
  ZPageAge prev_age = _age;
  _age = age;
  _last_used = 0;

  _generation_id = age == ZPageAge::old
      ? ZGenerationId::old
      : ZGenerationId::young;

  reset_seqnum();

  // Flip aged pages are still filled with the same objects, need to retain the top pointer.
  if (type != ZPageResetType::FlipAging) {
    _top = start();
  }

  reset_remembered_set(prev_age, type);

  if (type != ZPageResetType::InPlaceRelocation || (prev_age != ZPageAge::old && age == ZPageAge::old)) {
    // Promoted in-place relocations reset the live map,
    // because they clone the page.
    _livemap.reset();
  }
}

void ZPage::finalize_reset_for_in_place_relocation() {
  // Now we're done iterating over the livemaps
  _livemap.reset();
}

void ZPage::reset_type_and_size(ZPageType type) {
  _type = type;
  _livemap.resize(object_max_count());
  _remembered_set.resize(size());
}

ZPage* ZPage::retype(ZPageType type) {
  assert(_type != type, "Invalid retype");
  reset_type_and_size(type);
  return this;
}

ZPage* ZPage::split(size_t split_of_size) {
  return split(type_from_size(split_of_size), split_of_size);
}

ZPage* ZPage::split_with_pmem(ZPageType type, const ZPhysicalMemory& pmem) {
  // Resize this page
  const ZVirtualMemory vmem = _virtual.split(pmem.size());

  reset_type_and_size(type_from_size(_virtual.size()));
  reset(_age, ZPageResetType::Splitting);

  assert(vmem.end() == _virtual.start(), "Should be consecutive");

  log_debug(gc, page)("Split page [" PTR_FORMAT ", " PTR_FORMAT ", " PTR_FORMAT "]",
      untype(vmem.start()),
      untype(vmem.end()),
      untype(_virtual.end()));

  // Create new page
  return new ZPage(type, vmem, pmem);
}

ZPage* ZPage::split(ZPageType type, size_t split_of_size) {
  assert(_virtual.size() > split_of_size, "Invalid split");

  const ZPhysicalMemory pmem = _physical.split(split_of_size);

  return split_with_pmem(type, pmem);
}

ZPage* ZPage::split_committed() {
  // Split any committed part of this page into a separate page,
  // leaving this page with only uncommitted physical memory.
  const ZPhysicalMemory pmem = _physical.split_committed();
  if (pmem.is_null()) {
    // Nothing committed
    return NULL;
  }

  assert(!_physical.is_null(), "Should not be null");

  return split_with_pmem(type_from_size(pmem.size()), pmem);
}

class ZFindBaseOopClosure : public ObjectClosure {
private:
  volatile zpointer* _p;
  oop _result;

public:
  ZFindBaseOopClosure(volatile zpointer* p) :
      _p(p),
      _result(NULL) {}

  virtual void do_object(oop obj) {
    uintptr_t p_int = reinterpret_cast<uintptr_t>(_p);
    uintptr_t base_int = cast_from_oop<uintptr_t>(obj);
    uintptr_t end_int = base_int + wordSize * obj->size();
    if (p_int >= base_int && p_int < end_int) {
      _result = obj;
    }
  }

  oop result() const { return _result; }
};

void ZPage::clear_current_remembered() {
 _remembered_set.clear_current();
}

void ZPage::clear_previous_remembered() {
 _remembered_set.clear_previous();
}

void ZPage::log_msg(const char* msg_format, ...) const {
  LogTarget(Trace, gc, page) target;
  if (target.is_enabled()) {
    va_list argp;
    va_start(argp, msg_format);
    LogStream stream(target);
    print_on_msg(&stream, err_msg(FormatBufferDummy(), msg_format, argp));
    va_end(argp);
  }
}

void ZPage::print_on_msg(outputStream* out, const char* msg) const {
  out->print_cr(" %-6s  " PTR_FORMAT " " PTR_FORMAT " " PTR_FORMAT " %s/%-4u %s%s%s",
                type_to_string(), untype(start()), untype(top()), untype(end()),
                is_young() ? "Y" : "O",
                seqnum(),
                is_allocating()  ? " Allocating " : "",
                is_relocatable() ? " Relocatable" : "",
                msg == NULL ? "" : msg);
}

void ZPage::print_on(outputStream* out) const {
  print_on_msg(out, NULL);
}

void ZPage::print() const {
  print_on(tty);
}

void ZPage::verify_live(uint32_t live_objects, size_t live_bytes, bool in_place) const {
  if (!in_place) {
    // In-place relocation has changed the page to allocating
    assert_zpage_mark_state();
  }
  guarantee(live_objects == _livemap.live_objects(), "Invalid number of live objects");
  guarantee(live_bytes == _livemap.live_bytes(), "Invalid number of live bytes");
}

/*
 * Copyright (c) 2018, 2021, Oracle and/or its affiliates. All rights reserved.
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
#include "gc/z/zBarrierSet.hpp"
#include "gc/z/zBarrierSetAssembler.hpp"
#include "gc/z/zBarrierSetNMethod.hpp"
#include "gc/z/zGeneration.inline.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zHeap.inline.hpp"
#include "gc/z/zStackWatermark.hpp"
#include "gc/z/zThreadLocalData.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/registerMap.hpp"
#include "runtime/stackWatermarkSet.hpp"
#include "runtime/thread.hpp"
#include "utilities/macros.hpp"
#ifdef COMPILER1
#include "gc/z/c1/zBarrierSetC1.hpp"
#endif
#ifdef COMPILER2
#include "gc/z/c2/zBarrierSetC2.hpp"
#endif

class ZBarrierSetC1;
class ZBarrierSetC2;

ZBarrierSet::ZBarrierSet() :
    BarrierSet(make_barrier_set_assembler<ZBarrierSetAssembler>(),
               make_barrier_set_c1<ZBarrierSetC1>(),
               make_barrier_set_c2<ZBarrierSetC2>(),
               new ZBarrierSetNMethod(),
               BarrierSet::FakeRtti(BarrierSet::ZBarrierSet)) {}

ZBarrierSetAssembler* ZBarrierSet::assembler() {
  BarrierSetAssembler* const bsa = BarrierSet::barrier_set()->barrier_set_assembler();
  return reinterpret_cast<ZBarrierSetAssembler*>(bsa);
}

bool ZBarrierSet::barrier_needed(DecoratorSet decorators, BasicType type) {
  assert((decorators & AS_RAW) == 0, "Unexpected decorator");
  //assert((decorators & ON_UNKNOWN_OOP_REF) == 0, "Unexpected decorator");

  if (is_reference_type(type)) {
    assert((decorators & (IN_HEAP | IN_NATIVE)) != 0, "Where is reference?");
    // Barrier needed even when IN_NATIVE, to allow concurrent scanning.
    return true;
  }

  // Barrier not needed
  return false;
}

void ZBarrierSet::on_thread_create(Thread* thread) {
  // Create thread local data
  ZThreadLocalData::create(thread);
}

void ZBarrierSet::on_thread_destroy(Thread* thread) {
  // Destroy thread local data
  ZThreadLocalData::destroy(thread);
}

void ZBarrierSet::on_thread_attach(Thread* thread) {
  // Set thread local masks
  ZThreadLocalData::set_load_bad_mask(thread, ZPointerLoadBadMask);
  ZThreadLocalData::set_load_good_mask(thread, ZPointerLoadGoodMask);
  ZThreadLocalData::set_mark_bad_mask(thread, ZPointerMarkBadMask);
  ZThreadLocalData::set_store_bad_mask(thread, ZPointerStoreBadMask);
  ZThreadLocalData::set_store_good_mask(thread, ZPointerStoreGoodMask);
  ZThreadLocalData::set_nmethod_disarmed(thread, ZPointerStoreGoodMask);
  if (thread->is_Java_thread()) {
    JavaThread* const jt = JavaThread::cast(thread);
    StackWatermark* const watermark = new ZStackWatermark(jt);
    StackWatermarkSet::add_watermark(jt, watermark);
    ZThreadLocalData::store_barrier_buffer(jt)->initialize();
  }
}

void ZBarrierSet::on_thread_detach(Thread* thread) {
  // Flush and free any remaining mark stacks
  ZGeneration::young()->mark_flush_and_free(thread);
  ZGeneration::old()->mark_flush_and_free(thread);
}

void ZBarrierSet::on_slowpath_allocation_exit(JavaThread* thread, oop new_obj) {
  if (ZHeap::heap()->is_old(to_zaddress(new_obj))) {
    RegisterMap reg_map(thread, false);
    frame runtime_frame = thread->last_frame();
    assert(runtime_frame.is_runtime_frame(), "must be runtime frame");
    frame caller_frame = runtime_frame.sender(&reg_map);
    assert(caller_frame.is_compiled_frame(), "must be compiled");
    nmethod* nm = caller_frame.cb()->as_nmethod();
    if (nm->is_compiled_by_c2()) {
      // We promised C2 that its allocations would end up in young gen. This object
      // breaks that promise. Take a few steps in the interpreter instead, which has
      // no such assumptions about where an object resides.
      if (!caller_frame.is_deoptimized_frame()) {
        Deoptimization::deoptimize_frame(thread, caller_frame.id());
      }
    }
  }
}

void ZBarrierSet::print_on(outputStream* st) const {
  st->print_cr("ZBarrierSet");
}

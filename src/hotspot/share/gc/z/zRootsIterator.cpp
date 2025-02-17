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
#include "classfile/classLoaderDataGraph.hpp"
#include "gc/shared/oopStorageSetParState.inline.hpp"
#include "gc/z/zNMethod.hpp"
#include "gc/z/zNMethodTable.hpp"
#include "gc/z/zRootsIterator.hpp"
#include "gc/z/zStat.hpp"
#include "memory/resourceArea.hpp"
#include "prims/jvmtiTagMap.hpp"
#include "runtime/atomic.hpp"
#include "runtime/globals.hpp"
#include "runtime/safepoint.hpp"
#include "utilities/debug.hpp"

static const ZStatSubPhase ZSubPhaseConcurrentRootsOopStorageSet("Concurrent Roots OopStorageSet");
static const ZStatSubPhase ZSubPhaseConcurrentRootsClassLoaderDataGraph("Concurrent Roots ClassLoaderDataGraph");
static const ZStatSubPhase ZSubPhaseConcurrentRootsJavaThreads("Concurrent Roots JavaThreads");
static const ZStatSubPhase ZSubPhaseConcurrentRootsCodeCache("Concurrent Roots CodeCache");
static const ZStatSubPhase ZSubPhaseConcurrentWeakRootsOopStorageSet("Concurrent Weak Roots OopStorageSet");

template <typename Iterator>
template <typename ClosureType>
void ZParallelApply<Iterator>::apply(ClosureType* cl) {
  if (!Atomic::load(&_completed)) {
    _iter.apply(cl);
    if (!Atomic::load(&_completed)) {
      Atomic::store(&_completed, true);
    }
  }
}

ZOopStorageSetIteratorStrong::ZOopStorageSetIteratorStrong() :
    _iter() {}

void ZOopStorageSetIteratorStrong::apply(OopClosure* cl) {
  ZStatTimer timer(ZSubPhaseConcurrentRootsOopStorageSet);
  _iter.oops_do(cl);
}

void ZCLDsIteratorStrong::apply(CLDClosure* cl) {
  ZStatTimer timer(ZSubPhaseConcurrentRootsClassLoaderDataGraph);
  ClassLoaderDataGraph::always_strong_cld_do(cl);
}

void ZCLDsIteratorWeak::apply(CLDClosure* cl) {
  ZStatTimer timer(ZSubPhaseConcurrentRootsClassLoaderDataGraph);
  ClassLoaderDataGraph::roots_cld_do(NULL /* strong */, cl /* weak */);
}

void ZCLDsIteratorAll::apply(CLDClosure* cl) {
  ZStatTimer timer(ZSubPhaseConcurrentRootsClassLoaderDataGraph);
  ClassLoaderDataGraph::cld_do(cl);
}

ZJavaThreadsIterator::ZJavaThreadsIterator() :
    _threads(),
    _claimed(0) {}

uint ZJavaThreadsIterator::claim() {
  return Atomic::fetch_and_add(&_claimed, 1u);
}

void ZJavaThreadsIterator::apply(ThreadClosure* cl) {
  ZStatTimer timer(ZSubPhaseConcurrentRootsJavaThreads);

  // The resource mark is needed because interpreter oop maps are
  // not reused in concurrent mode. Instead, they are temporary and
  // resource allocated.
  ResourceMark                 _rm;

  for (uint i = claim(); i < _threads.length(); i = claim()) {
    cl->do_thread(_threads.thread_at(i));
  }
}

ZNMethodsIteratorImpl::ZNMethodsIteratorImpl(bool enabled, bool secondary)
    : _enabled(enabled), _secondary(secondary) {
  if (_enabled) {
    ZNMethod::nmethods_do_begin(secondary);
  }
}

ZNMethodsIteratorImpl::~ZNMethodsIteratorImpl() {
  if (_enabled) {
    ZNMethod::nmethods_do_end(_secondary);
  }
}

void ZNMethodsIteratorImpl::apply(NMethodClosure* cl) {
  ZStatTimer timer(ZSubPhaseConcurrentRootsCodeCache);
  ZNMethod::nmethods_do(_secondary, cl);
}

void ZRootsIteratorStrongColored::apply(OopClosure* cl,
                                  CLDClosure* cld_cl) {
  _oop_storage_set_strong.apply(cl);
  _clds_strong.apply(cld_cl);
}

void ZRootsIteratorStrongUncolored::apply(ThreadClosure* thread_cl,
                                          NMethodClosure* nm_cl) {
  _java_threads.apply(thread_cl);
  if (!ClassUnloading) {
    _nmethods_strong.apply(nm_cl);
  }
}

void ZRootsIteratorWeakUncolored::apply(NMethodClosure* nm_cl) {
  _nmethods_weak.apply(nm_cl);
}

ZOopStorageSetIteratorWeak::ZOopStorageSetIteratorWeak() :
    _iter() {}

void ZOopStorageSetIteratorWeak::apply(OopClosure* cl) {
  ZStatTimer timer(ZSubPhaseConcurrentWeakRootsOopStorageSet);
  _iter.oops_do(cl);
}

void ZOopStorageSetIteratorWeak::report_num_dead() {
  _iter.report_num_dead();
}

void ZRootsIteratorWeakColored::report_num_dead() {
  _oop_storage_set_weak.iter().report_num_dead();
}

void ZRootsIteratorWeakColored::apply(OopClosure* cl) {
  _oop_storage_set_weak.apply(cl);
}

void ZRootsIteratorAllColored::apply(OopClosure* cl,
                                     CLDClosure* cld_cl) {
  _oop_storage_set_strong.apply(cl);
  _oop_storage_set_weak.apply(cl);
  _clds_all.apply(cld_cl);
}

void ZRootsIteratorAllUncolored::apply(ThreadClosure* thread_cl,
                                       NMethodClosure* nm_cl) {
  _java_threads.apply(thread_cl);
  _nmethods_all.apply(nm_cl);
}

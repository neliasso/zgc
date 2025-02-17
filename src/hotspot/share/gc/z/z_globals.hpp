/*
 * Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_Z_Z_GLOBALS_HPP
#define SHARE_GC_Z_Z_GLOBALS_HPP

#define GC_Z_FLAGS(develop,                                                 \
                   develop_pd,                                              \
                   product,                                                 \
                   product_pd,                                              \
                   notproduct,                                              \
                   range,                                                   \
                   constraint)                                              \
                                                                            \
  product(double, ZAllocationSpikeTolerance, 2.0,                           \
          "Allocation spike tolerance factor")                              \
                                                                            \
  product(double, ZFragmentationLimit, 25.0,                                \
          "Maximum allowed heap fragmentation")                             \
                                                                            \
  product(size_t, ZMarkStackSpaceLimit, 8*G,                                \
          "Maximum number of bytes allocated for mark stacks")              \
          range(32*M, 1024*G)                                               \
                                                                            \
  product(double, ZCollectionIntervalMinor, -1,                             \
          "Force Minor GC at a fixed time interval (in seconds)")           \
                                                                            \
  product(double, ZCollectionIntervalMajor, -1,                             \
          "Force GC at a fixed time interval (in seconds)")                 \
                                                                            \
  product(bool, ZCollectionIntervalOnly, false,                             \
          "Only use timers for GC heuristics")                              \
                                                                            \
  product(bool, ZProactive, true,                                           \
          "Enable proactive GC cycles")                                     \
                                                                            \
  product(bool, ZUncommit, true,                                            \
          "Uncommit unused memory")                                         \
                                                                            \
  product(bool, ZBufferStoreBarriers, true,                                 \
          "Buffer store barriers")                                          \
                                                                            \
  product(uintx, ZUncommitDelay, 5 * 60,                                    \
          "Uncommit memory if it has been unused for the specified "        \
          "amount of time (in seconds)")                                    \
                                                                            \
  product(uintx, ZIndexDistributorStrategy, 0,                              \
          "Strategy used to distribute indices to parallel workers "        \
          "0: Claim tree "                                                  \
          "1: Simple Striped ")                                             \
                                                                            \
  product(uintx, ZRelocateRemsetStrategy, 1,                                \
          "Strategy used to add remset entries to promoted objects "        \
          "0: Add for all fields during relocation "                        \
          "1: Filter and remapping during relocation "                      \
          "2: Filter and remapping after relocation ")                      \
          /* This flag exists so that these approaches can be evaluated */  \
          /* 0 - 1: Unclear if one is better than the other */              \
          /* 2: has been shown to be very expensive */                      \
                                                                            \
  product(uint, ZStatisticsInterval, 10, DIAGNOSTIC,                        \
          "Time between statistics print outs (in seconds)")                \
          range(1, (uint)-1)                                                \
                                                                            \
  product(bool, ZStressRelocateInPlace, false, DIAGNOSTIC,                  \
          "Always relocate pages in-place")                                 \
                                                                            \
  product(bool, ZVerifyRoots, trueInDebug, DIAGNOSTIC,                      \
          "Verify roots")                                                   \
                                                                            \
  product(bool, ZVerifyObjects, false, DIAGNOSTIC,                          \
          "Verify objects")                                                 \
                                                                            \
  develop(bool, ZVerifyOops, true,                                          \
          "Verify accessed oops")                                           \
                                                                            \
  product(bool, ZVerifyMarking, trueInDebug, DIAGNOSTIC,                    \
          "Verify marking stacks")                                          \
                                                                            \
  product(bool, ZVerifyForwarding, false, DIAGNOSTIC,                       \
          "Verify forwarding tables")

// end of GC_Z_FLAGS

#endif // SHARE_GC_Z_Z_GLOBALS_HPP

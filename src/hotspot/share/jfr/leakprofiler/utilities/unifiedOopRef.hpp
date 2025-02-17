/*
 * Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#ifndef SHARE_JFR_LEAKPROFILER_UTILITIES_UNIFIEDOOPREF_HPP
#define SHARE_JFR_LEAKPROFILER_UTILITIES_UNIFIEDOOPREF_HPP

#include "oops/oopsHierarchy.hpp"
#include "utilities/globalDefinitions.hpp"

struct UnifiedOopRef {
  uintptr_t _value;

  template <typename T>
  T addr() const;

  bool is_narrow() const;
  bool is_native() const;
  bool is_non_barriered() const;
  bool is_null() const;

  oop dereference() const;

  static UnifiedOopRef encode_in_native(const narrowOop* ref);
  static UnifiedOopRef encode_in_native(const oop* ref);
  static UnifiedOopRef encode_non_barriered(const oop* ref);
  static UnifiedOopRef encode_in_heap(const oop* ref);
  static UnifiedOopRef encode_in_heap(const narrowOop* ref);
  static UnifiedOopRef encode_null();
};

#endif // SHARE_JFR_LEAKPROFILER_UTILITIES_UNIFIEDOOPREF_HPP

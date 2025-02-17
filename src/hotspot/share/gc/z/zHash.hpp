/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_Z_ZHASH_HPP
#define SHARE_GC_Z_ZHASH_HPP

#include "gc/z/zAddress.hpp"
#include "memory/allocation.hpp"
#include "utilities/globalDefinitions.hpp"

class ZHash : public AllStatic {
public:
  static uint32_t uint32_to_uint32(uint32_t key);
  static uint32_t address_to_uint32(uintptr_t key);
  static uint32_t offset_to_uint32(zoffset key);
};

#endif // SHARE_GC_Z_ZHASH_HPP

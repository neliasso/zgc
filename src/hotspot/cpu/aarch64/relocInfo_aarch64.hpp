/*
 * Copyright (c) 1997, 2020, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2014, Red Hat Inc. All rights reserved.
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

#ifndef CPU_AARCH64_RELOCINFO_AARCH64_HPP
#define CPU_AARCH64_RELOCINFO_AARCH64_HPP

  // machine-dependent parts of class relocInfo
 private:
  enum {
    // Relocations are byte-aligned.
    offset_unit        =  1,
    // Must be at least 1 for RelocInfo::narrow_oop_in_const.
    // Must be at least 2 for ZGC GC barrier patching
    format_width       =  2
  };

 public:

  // This platform has no oops in the code that are not also
  // listed in the oop section.
  static bool mustIterateImmediateOopsInCode() { return false; }

#endif // CPU_AARCH64_RELOCINFO_AARCH64_HPP

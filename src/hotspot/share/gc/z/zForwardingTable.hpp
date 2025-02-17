/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_Z_ZFORWARDINGTABLE_HPP
#define SHARE_GC_Z_ZFORWARDINGTABLE_HPP

#include "gc/z/zGranuleMap.hpp"
#include "gc/z/zIndexDistributor.hpp"

class ZForwarding;

class ZForwardingTable {
  friend class VMStructs;
  friend class ZOldGenerationPagesSafeIterator;
  friend class ZForwardingTableParallelIterator;

private:
  ZGranuleMap<ZForwarding*> _map;

  ZForwarding* at(size_t index) const;

public:
  ZForwardingTable();

  ZForwarding* get(zaddress_unsafe addr) const;

  void insert(ZForwarding* forwarding);
  void remove(ZForwarding* forwarding);
};

class ZForwardingTableParallelIterator : public StackObj {
private:
  const ZForwardingTable* _table;
  ZIndexDistributor       _index_distributor;

public:
  ZForwardingTableParallelIterator(const ZForwardingTable* table);

  template <typename Function>
  void do_forwardings(Function function);
};

#endif // SHARE_GC_Z_ZFORWARDINGTABLE_HPP

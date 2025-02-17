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

#ifndef SHARE_GC_Z_ZDRIVERPORT_HPP
#define SHARE_GC_Z_ZDRIVERPORT_HPP

#include "gc/shared/gcCause.hpp"
#include "gc/z/zList.hpp"
#include "gc/z/zLock.hpp"

class ZDriverPortEntry;

class ZDriverRequest {
private:
  GCCause::Cause _cause;
  uint           _young_nworkers;
  uint           _old_nworkers;

public:
  ZDriverRequest();
  ZDriverRequest(GCCause::Cause cause, uint young_nworkers, uint old_nworkers);

  bool operator==(const ZDriverRequest& other) const;

  GCCause::Cause cause() const;
  uint young_nworkers() const;
  uint old_nworkers() const;
};

class ZDriverPort {
private:
  mutable ZConditionLock  _lock;
  bool                    _has_message;
  ZDriverRequest          _message;
  uint64_t                _seqnum;
  ZList<ZDriverPortEntry> _queue;

public:
  ZDriverPort();

  bool is_busy() const;

  // For use by sender
  void send_sync(const ZDriverRequest& request);
  void send_async(const ZDriverRequest& request);

  // For use by receiver
  ZDriverRequest receive();
  void ack();
};

#endif // SHARE_GC_Z_ZDRIVERPORT_HPP

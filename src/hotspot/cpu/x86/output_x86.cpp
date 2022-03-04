/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "c2_intelJccErratum_x86.hpp"
#include "opto/output.hpp"

void perform_peeping();

 void PhaseOutput::pd_perform_mach_node_analysis() {
  if (VM_Version::has_intel_jcc_erratum()) {
    int extra_padding = IntelJccErratum::tag_affected_machnodes(C, C->cfg(), C->regalloc());
    _buf_sizes._code += extra_padding;
  }
  if (UseZGC && BarrierNullCheckElimination) {
    perform_peeping();
  }
}

void perform_peeping() {
  Compile* const C = Compile::current();
  PhaseCFG* const cfg = C->cfg();
  for (uint i = 0; i < cfg->number_of_blocks(); ++i) {
    Block* const block = cfg->get_block(i);
    for (uint j = 0; j < block->number_of_nodes(); ++j) {
      Node* const node = block->get_node(j);
      MachNode* mn = node->isa_Mach();
      if (mn == nullptr) {
        continue;
      }

        if (mn->rule() == MachOpcodes::zLoadP_rule) {
        // Ensure that there are at least 2 more instructions, a machproj and a testreg
        if (j >= (block->number_of_nodes() - 2)) {
          continue; // no room
        }

        // Succeeded by a connected MachProj
        MachProjNode* proj = block->get_node(j + 1)->isa_MachProj();
        if ((proj == nullptr) || (proj->in(0) != mn)) {
          continue;
        }

        // Succeeded by a connected testP_reg node
        MachNode* const test = block->get_node(j + 2)->isa_Mach();
        if ((test == nullptr) || (test->rule() != MachOpcodes::testP_reg_rule)) {
          continue;
        }

        // TODO replace with tests for matching registers
        // TODO - allow instructions in between - how common?
        // Must be connected to load
        if (test->in(1) != mn) {
          continue;
        }

        // Record that the stub need to recreate ZF flag
        mn->add_barrier_data(ZBarrierNullCheckRemoval);

        // Drop node
        assert(block->find_node(test) == (j + 2), "check");
        block->remove_node(j + 2);
      }
    }
  }
}
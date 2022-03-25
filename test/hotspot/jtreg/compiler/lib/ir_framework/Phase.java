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
 */

package compiler.lib.ir_framework;

import java.util.HashMap;
import java.util.Map;

public enum Phase {
    // String argument have to match the ideal tag that is printed by Hotspot into the hotspot_pid file.
    // See blocks that check for Compile::print_ideal().
    AFTER_PARSING("After Parsing"),         // Added by IR framework
    AFTER_OPTIMIZATIONS("Before matching"), // Default tag when specifying PrintIdeal
    FINAL_CODE("Final Code"),               // Added by IR framework
    PRINT_SCHEDULING("Print Scheduling"),   // Added by IR framework
    MACHANALYSIS("After mach analysis");

    private final String tag;
    private static final Map<String, Phase> PHASES_BY_LABEL = new HashMap<>();

    static {
        for (Phase phase : Phase.values()) {
            PHASES_BY_LABEL.put(phase.tag, phase);
        }
    }

    Phase(String tag) {
        this.tag = tag;
    }

    public String getTag() {
        return tag;
    }

    public static Phase valueOfTag(String tag) {
        return PHASES_BY_LABEL.get(tag);
    }
}

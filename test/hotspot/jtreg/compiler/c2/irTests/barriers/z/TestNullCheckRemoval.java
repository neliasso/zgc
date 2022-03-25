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

package compiler.c2.irTests.barriers.z;

import compiler.lib.ir_framework.*;

/*
 * @test
 * @summary Test elision of dominating barriers in ZGC
 * @library /test/lib /
 * @requires vm.gc.Z
 * @run driver compiler.c2.irTests.barriers.z.TestNullCheckRemoval
 */

class Payload {
    Content c;

    public Payload(Content c) {
        this.c = c;
    }
}

class Content {
    public int id;

    public Content(int id) {
        this.id = id;
    }
}

public class TestNullCheckRemoval {

    Payload p = new Payload(new Content(5));
    Payload pn = new Payload(null);
    Content c1 = new Content(45);
    Content c2 = new Content(15);

    public static void main(String[] args) {
        TestFramework framework = new TestFramework();
        Scenario zgc = new Scenario(0, "-XX:+UseZGC", "-XX:+UseBarrierNullCheckElimination", "-XX:CompileCommand=dontinline,*::blackhole");
        framework.addScenarios(zgc).start();
    }

    static void blackhole(Content t) {
        // nothing
    }

    static void blackhole(Payload p) {
        // nothing
    }

    static void blackhole(Payload p, Content t) {
        // nothing
    }

    @Test
    @IR(counts = { IRNode.ZLOAD_P_NULLCHECK_ELIDED, "1" }, phase = Phase.MACHANALYSIS)
    @IR(counts = { IRNode.TESTP_REG, "1" }, phase = Phase.MACHANALYSIS)
    private static int testNullCheck(Payload p) {
        if (p.c == null) {
            return 5;
        } else {
            return 7;
        }
    }

    @Run(test = {"testNullCheck"})
    private void testBasic_runner() {
        testNullCheck(p);
        //testNullCheck(pn);
    }
}

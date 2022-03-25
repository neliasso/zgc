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
 * @run driver compiler.c2.irTests.barriers.z.TestDomBarrierElision
 */

class Payload {
    Content c;

    public Payload(Content c) {
        this.c = c;
    }

    public Payload() {
        // TODO Auto-generated constructor stub
    }
}

class Content {
    int id;
    public Content(int id) {
        this.id = id;
    }
}

public class TestDomBarrierElision {

    Payload p = new Payload(new Content(5));
    Content c1 = new Content(45);
    Content c2 = new Content(15);

    public static void main(String[] args) {
        TestFramework framework = new TestFramework();
        Scenario zgc = new Scenario(0, "-XX:+UseZGC", "-XX:CompileCommand=dontinline,*::blackhole");
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
    @IR(counts = { IRNode.ZLOAD_P,  "1" }, phase = Phase.FINAL_CODE)
    private static Content testBasicLoad(Payload p) {
        return p.c;
    }

    @Test
    @IR(counts = { IRNode.ZSTORE_P,  "1" }, phase = Phase.FINAL_CODE)
    private static Content testBasicStore(Payload p, Content c1) {
        return p.c = c1;
    }

    @Test
    @IR(counts = { IRNode.ZLOAD_P,  "1" }, phase = Phase.FINAL_CODE)
    @IR(counts = { IRNode.ZLOAD_P_ELIDED,  "1" }, phase = Phase.FINAL_CODE)
    private static Content testBasicLoadDom(Payload p) {
        Content t = p.c;
        blackhole(t);
        return p.c;
    }

    @Test
    @IR(counts = { IRNode.ZSTORE_P,  "1" }, phase = Phase.FINAL_CODE)
    @IR(counts = { IRNode.ZSTORE_P_ELIDED,  "1" }, phase = Phase.FINAL_CODE)
    private static void testBasicStoreDom(Payload p, Content t1, Content t2) {
        p.c = t1;
        blackhole(p);
        p.c = t2;
    }

    @Test
    @IR(counts = { IRNode.ZSTORE_P,  "1" }, phase = Phase.FINAL_CODE)
    @IR(counts = { IRNode.ZLOAD_P_ELIDED, "1" }, phase = Phase.FINAL_CODE)
    private static Content testBasicStoreLoadDom(Payload p, Content t1) {
        p.c = t1;
        blackhole(p);
        return p.c;
    }

    @Test
    @IR(counts = { IRNode.ZLOAD_P, "1" }, phase = Phase.FINAL_CODE)
    @IR(counts = { IRNode.ZSTORE_P,"1" }, phase = Phase.FINAL_CODE)
    private static void testBasicLoadStoreDom(Payload p, Content t1) {
        Content t = p.c;
        blackhole(t);
        p.c = t1;
    }

    @Test
    @IR(counts = { IRNode.ZLOAD_P, "1" }, phase = Phase.FINAL_CODE)
    @IR(counts = { IRNode.ZLOAD_P_ELIDED, "2" }, phase = Phase.FINAL_CODE)
    private static Content testLoadDomShortLoop(Payload p) {
        Content t = p.c;
        blackhole(p, t);
        for (int i = 0 ; i < 5; i++) {
            t = p.c;
            blackhole(p, t);
        }
        return p.c;
    }

    @Test
    @IR(counts = { IRNode.ZLOAD_P, "1" }, phase = Phase.FINAL_CODE)
    @IR(counts = { IRNode.ZLOAD_P_ELIDED, "2" }, phase = Phase.FINAL_CODE)
    private static Content testLoadDomLongLoop(Payload p) {
        Content t = p.c;
        blackhole(p, t);
        for (int i = 0 ; i < 1024; i++) {
            t = p.c;
            blackhole(p, t);
        }
        return p.c;
    }

    @Test
    @IR(counts = { IRNode.ZLOAD_P_ELIDED,  "1" }, phase = Phase.FINAL_CODE)
    private static Content testAlloctionDomLoad() {
        Payload p = new Payload();
        blackhole(p);
        return p.c;
    }

    @Test
    @IR(counts = { IRNode.ZSTORE_P_ELIDED,  "1" }, phase = Phase.FINAL_CODE)
    private static void testAlloctionDomStore(Content c) {
        Payload p = new Payload();
        blackhole(p);
        p.c = c;
    }

    @Run(test = {"testAlloctionDomLoad",
                 "testAlloctionDomStore"})
    private void testAllocationDom_runner() {
        testAlloctionDomLoad();
        testAlloctionDomStore(c1);
    }

    @Run(test = {"testLoadDomShortLoop",
                 "testLoadDomLongLoop"})
    private void testLoadDomShortLoop_runner() {
        testLoadDomShortLoop(p);
        testLoadDomLongLoop(p);
    }

    @Run(test = {"testBasicLoad",
                "testBasicStore",
                "testBasicLoadDom",
                "testBasicStoreDom",
                "testBasicStoreLoadDom",
                "testBasicLoadStoreDom"})
    private void testBasic_runner() {
        testBasicLoad(p);
        testBasicStore(p, c1);
        testBasicLoadDom(p);
        testBasicStoreDom(p, c1, c2);
        testBasicStoreLoadDom(p, c1);
        testBasicLoadStoreDom(p, c1);
    }
}

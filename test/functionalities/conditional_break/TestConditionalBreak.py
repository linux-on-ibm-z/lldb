"""
Test conditionally break on a function and inspect its variables.
"""

import os, time
import re
import unittest2
import lldb, lldbutil
from lldbtest import *

# rdar://problem/8532131
# lldb not able to digest the clang-generated debug info correctly with respect to function name
#
# This class currently fails for clang as well as llvm-gcc.

class ConditionalBreakTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @skipUnlessDarwin
    @python_api_test
    @dsym_test
    def test_with_dsym_python(self):
        """Exercise some thread and frame APIs to break if c() is called by a()."""
        self.buildDsym()
        self.do_conditional_break()

    @python_api_test
    @dwarf_test
    def test_with_dwarf_python(self):
        """Exercise some thread and frame APIs to break if c() is called by a()."""
        self.buildDwarf()
        self.do_conditional_break()

    @skipUnlessDarwin
    @dsym_test
    def test_with_dsym_command(self):
        """Simulate a user using lldb commands to break on c() if called from a()."""
        self.buildDsym()
        self.simulate_conditional_break_by_user()

    @dwarf_test
    def test_with_dwarf_command(self):
        """Simulate a user using lldb commands to break on c() if called from a()."""
        self.buildDwarf()
        self.simulate_conditional_break_by_user()

    def do_conditional_break(self):
        """Exercise some thread and frame APIs to break if c() is called by a()."""
        exe = os.path.join(os.getcwd(), "a.out")

        target = self.dbg.CreateTarget(exe)
        self.assertTrue(target, VALID_TARGET)

        breakpoint = target.BreakpointCreateByName("c", exe)
        self.assertTrue(breakpoint, VALID_BREAKPOINT)

        # Now launch the process, and do not stop at entry point.
        process = target.LaunchSimple (None, None, self.get_process_working_directory())

        self.assertTrue(process, PROCESS_IS_VALID)

        # The stop reason of the thread should be breakpoint.
        self.assertTrue(process.GetState() == lldb.eStateStopped,
                        STOPPED_DUE_TO_BREAKPOINT)

        # Find the line number where a's parent frame function is c.
        line = line_number('main.c',
            "// Find the line number where c's parent frame is a here.")

        # Suppose we are only interested in the call scenario where c()'s
        # immediate caller is a() and we want to find out the value passed from
        # a().
        #
        # The 10 in range(10) is just an arbitrary number, which means we would
        # like to try for at most 10 times.
        for j in range(10):
            if self.TraceOn():
                print "j is: ", j
            thread = process.GetThreadAtIndex(0)
            
            if thread.GetNumFrames() >= 2:
                frame0 = thread.GetFrameAtIndex(0)
                name0 = frame0.GetFunction().GetName()
                frame1 = thread.GetFrameAtIndex(1)
                name1 = frame1.GetFunction().GetName()
                #lldbutil.print_stacktrace(thread)
                self.assertTrue(name0 == "c", "Break on function c()")
                if (name1 == "a"):
                    # By design, we know that a() calls c() only from main.c:27.
                    # In reality, similar logic can be used to find out the call
                    # site.
                    self.assertTrue(frame1.GetLineEntry().GetLine() == line,
                                    "Immediate caller a() at main.c:%d" % line)

                    # And the local variable 'val' should have a value of (int) 3.
                    val = frame1.FindVariable("val")
                    self.assertTrue(val.GetTypeName() == "int", "'val' has int type")
                    self.assertTrue(val.GetValue() == "3", "'val' has a value of 3")
                    break

            process.Continue()

    def simulate_conditional_break_by_user(self):
        """Simulate a user using lldb commands to break on c() if called from a()."""

        # Sourcing .lldb in the current working directory, which sets the main
        # executable, sets the breakpoint on c(), and adds the callback for the
        # breakpoint such that lldb only stops when the caller of c() is a().
        # the "my" package that defines the date() function.
        if self.TraceOn():
            print "About to source .lldb"

        if not self.TraceOn():
            self.HideStdout()

        # Separate out the "file a.out" command from .lldb file, for the sake of
        # remote testsuite.
        self.runCmd("file a.out")
        self.runCmd("command source .lldb")

        self.runCmd ("break list")

        if self.TraceOn():
            print "About to run."
        self.runCmd("run", RUN_SUCCEEDED)

        self.runCmd ("break list")

        if self.TraceOn():
            print "Done running"

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped', 'stop reason = breakpoint'])

        # The frame info for frame #0 points to a.out`c and its immediate caller
        # (frame #1) points to a.out`a.

        self.expect("frame info", "We should stop at c()",
            substrs = ["a.out`c"])

        # Select our parent frame as the current frame.
        self.runCmd("frame select 1")
        self.expect("frame info", "The immediate caller should be a()",
            substrs = ["a.out`a"])



if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()

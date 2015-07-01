"""
Test that we are able to properly report a usable dynamic type
"""

import os, time
import unittest2
import lldb
from lldbtest import *
import lldbutil

@skipUnlessDarwin
class ObjCDynamicSBTypeTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @dsym_test
    @skipIfi386
    def test_dyn_with_dsym(self):
        """Test that we are able to properly report a usable dynamic type."""
        d = {'EXE': self.exe_name}
        self.buildDsym(dictionary=d)
        self.setTearDownCleanup(dictionary=d)
        self.dyn(self.exe_name)

    @dwarf_test
    @skipIfi386
    def test_dyn_with_dwarf(self):
        """Test that we are able to properly report a usable dynamic type."""
        d = {'EXE': self.exe_name}
        self.buildDwarf(dictionary=d)
        self.setTearDownCleanup(dictionary=d)
        self.dyn(self.exe_name)

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # We'll use the test method name as the exe_name.
        self.exe_name = self.testMethodName
        # Find the line number to break inside main().
        self.main_source = "main.m"
        self.line = line_number(self.main_source, '// Set breakpoint here.')

    def dyn(self, exe_name):
        """Test that we are able to properly report a usable dynamic type."""
        exe = os.path.join(os.getcwd(), exe_name)
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        lldbutil.run_break_set_by_file_and_line (self, self.main_source, self.line, num_expected_locations=1, loc_exact=True)

        self.runCmd("run", RUN_SUCCEEDED)

        v_object = self.frame().FindVariable("object").GetDynamicValue(lldb.eDynamicCanRunTarget)
        v_base = self.frame().FindVariable("base").GetDynamicValue(lldb.eDynamicCanRunTarget)
        self.assertTrue(v_object.GetTypeName() == "MyDerivedClass *", "The NSObject is properly type-named")
        self.assertTrue(v_base.GetTypeName() == "MyDerivedClass *", "The Base is properly type-named")
        object_type = v_object.GetType()
        base_type = v_base.GetType()
        self.assertTrue(object_type.GetName() == "MyDerivedClass *", "The dynamic SBType for NSObject is for the correct type")
        self.assertTrue(base_type.GetName() == "MyDerivedClass *", "The dynamic SBType for Base is for the correct type")
        object_pointee_type = object_type.GetPointeeType()
        base_pointee_type = base_type.GetPointeeType()
        self.assertTrue(object_pointee_type.GetName() == "MyDerivedClass", "The dynamic type for NSObject figures out its pointee type just fine")
        self.assertTrue(base_pointee_type.GetName() == "MyDerivedClass", "The dynamic type for Base figures out its pointee type just fine")

        self.assertTrue(object_pointee_type.GetDirectBaseClassAtIndex(0).GetName() == "MyBaseClass", "The dynamic type for NSObject can go back to its base class")
        self.assertTrue(base_pointee_type.GetDirectBaseClassAtIndex(0).GetName() == "MyBaseClass", "The dynamic type for Base can go back to its base class")

        self.assertTrue(object_pointee_type.GetDirectBaseClassAtIndex(0).GetType().GetDirectBaseClassAtIndex(0).GetName() == "NSObject", "The dynamic type for NSObject can go up the hierarchy")
        self.assertTrue(base_pointee_type.GetDirectBaseClassAtIndex(0).GetType().GetDirectBaseClassAtIndex(0).GetName() == "NSObject", "The dynamic type for Base can go up the hierarchy")

        self.assertTrue(object_pointee_type.GetNumberOfFields() == 2, "The dynamic type for NSObject has 2 fields")
        self.assertTrue(base_pointee_type.GetNumberOfFields() == 2, "The dynamic type for Base has 2 fields")

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()

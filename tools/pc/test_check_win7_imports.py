#!/usr/bin/env python3
"""check_win7_imports.py: what counts as newer than Windows 7, and the import
table read from a real build when one is there (tmp/pc/win32)."""
import os, sys, unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_win7_imports as check

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(ROOT, "tmp", "pc", "win32", "memories-pc.exe")


class NewerThanWin7(unittest.TestCase):
    def test_windows_8_function(self):
        found = check.newer_than_win7({"kernel32.dll": {"CreateFileW", "GetCurrentThreadStackLimits"}})
        self.assertEqual(found, ["kernel32.dll!GetCurrentThreadStackLimits"])

    def test_windows_8_dll(self):
        self.assertEqual(check.newer_than_win7({"shcore.dll": {"GetDpiForMonitor"}}), ["shcore.dll"])

    def test_ucrt_and_windows_7_functions_pass(self):
        imports = {"kernel32.dll": {"CreateWaitableTimerExW", "CreateFileW"},
                   "api-ms-win-crt-runtime-l1-1-0.dll": {"_initterm"}, "winhttp.dll": {"WinHttpOpen"}}
        self.assertEqual(check.newer_than_win7(imports), [])

    def test_names_are_matched_whatever_the_dll_case(self):
        self.assertEqual(check.newer_than_win7({"KERNEL32.dll": {"GetSystemTimePreciseAsFileTime"}}),
                         ["kernel32.dll!GetSystemTimePreciseAsFileTime"])


class PeImports(unittest.TestCase):
    def test_not_a_pe_file(self):
        with self.assertRaises(ValueError):
            check.pe_imports(b"\x7fELF" + bytes(200))

    @unittest.skipUnless(os.path.exists(EXE), "no Windows build in tmp/pc/win32")
    def test_real_build(self):
        with open(EXE, "rb") as stream:
            imports = check.pe_imports(stream.read())
        self.assertIn("CreateFileW", imports["kernel32.dll"])
        self.assertIn("sdl3.dll", imports)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""The Windows games start on Windows 7: no import Windows 7 lacks.

Windows refuses to start an executable that imports a function its DLLs do
not have ("The procedure entry point ... could not be located"). This reads
the import table of every .exe and .dll given, or inside a given .zip, and
fails on the functions and DLLs below, which came with Windows 8 or later.
The list holds the ones a port like this could pick up, not every one. The
UCRT's api-ms-win-crt-* DLLs are fine: Windows 7 SP1 has them through
Windows Update (KB2999226) or the Visual C++ 2015+ redistributable.
Delay-loaded imports are not read: they do not stop the start.

  python3 tools/pc/check_win7_imports.py dist/yfm-redecomp-v1.0.0-windows.zip
"""
import struct, sys, zipfile

NEWER_FUNCTIONS = {
    "kernel32.dll": {
        "GetCurrentThreadStackLimits", "GetSystemTimePreciseAsFileTime", "CreateFile2", "CopyFile2",
        "SetThreadDescription", "GetThreadDescription", "WaitOnAddress", "WakeByAddressSingle",
        "WakeByAddressAll", "GetOverlappedResultEx", "PrefetchVirtualMemory", "SetThreadInformation",
        "GetThreadInformation", "SetProcessInformation", "GetProcessInformation", "DiscardVirtualMemory",
        "OfferVirtualMemory", "ReclaimVirtualMemory", "VirtualAlloc2", "MapViewOfFile3",
        "CreateFileMappingFromApp", "GetProcessMitigationPolicy", "SetProcessMitigationPolicy",
        "GetCurrentPackageId", "GetCurrentPackageFullName", "GetPackageFamilyName",
        "GetFileInformationByName", "GetSystemCpuSetInformation", "SetThreadSelectedCpuSets",
    },
    "user32.dll": {
        "GetDpiForWindow", "GetDpiForSystem", "SetProcessDpiAwarenessContext", "SetThreadDpiAwarenessContext",
        "AreDpiAwarenessContextsEqual", "GetAwarenessFromDpiAwarenessContext", "GetSystemMetricsForDpi",
        "AdjustWindowRectExForDpi", "EnableNonClientDpiScaling", "GetPointerInfo", "GetPointerType",
        "EnableMouseInPointer", "GetPointerFrameInfo",
    },
}
NEWER_DLLS = {"shcore.dll", "d3d12.dll", "dcomp.dll", "windows.storage.dll",
              "api-ms-win-core-synch-l1-2-0.dll", "api-ms-win-core-path-l1-1-0.dll",
              "api-ms-win-core-winrt-l1-1-0.dll", "api-ms-win-core-winrt-string-l1-1-0.dll"}


def pe_imports(data):
    """{dll name in lowercase: names imported from it} of a PE file's import
    table; an import by ordinal is '#<n>'. ValueError when it is no PE."""
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError("not a PE file")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE file")
    count, optional_size = struct.unpack_from("<H", data, pe + 6)[0], struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    pe64 = struct.unpack_from("<H", data, optional)[0] == 0x20B
    import_rva = struct.unpack_from("<I", data, optional + (112 if pe64 else 96) + 8)[0]
    sections = [struct.unpack_from("<8sIIII", data, optional + optional_size + 40 * i) for i in range(count)]

    def offset(rva):
        for _, size, address, raw_size, raw in sections:
            if address <= rva < address + max(size, raw_size):
                return raw + rva - address
        raise ValueError(f"RVA {rva:#x} is in no section")

    def text(rva):
        start = offset(rva)
        return data[start:data.index(b"\0", start)].decode("ascii")

    result = {}
    at = offset(import_rva) if import_rva else None
    while at is not None:
        lookup, _, _, name_rva, thunk = struct.unpack_from("<IIIII", data, at)
        if not name_rva:
            break
        names = result.setdefault(text(name_rva).lower(), set())
        entry, width = offset(lookup or thunk), 8 if pe64 else 4
        flag = 1 << (63 if pe64 else 31)
        while True:
            value = struct.unpack_from("<Q" if pe64 else "<I", data, entry)[0]
            if not value:
                break
            names.add(f"#{value & 0xFFFF}" if value & flag else text((value & 0x7FFFFFFF) + 2))
            entry += width
        at += 20
    return result


def newer_than_win7(imports):
    """'dll' or 'dll!function' for each import Windows 7 does not have, sorted."""
    found = []
    for dll, names in imports.items():
        dll = dll.lower()
        if dll in NEWER_DLLS:
            found.append(dll)
        found += [f"{dll}!{name}" for name in names & NEWER_FUNCTIONS.get(dll, set())]
    return sorted(found)


def binaries(path):
    """(name, bytes) of the file, or of each .exe/.dll in the .zip."""
    if path.lower().endswith(".zip"):
        with zipfile.ZipFile(path) as archive:
            for member in archive.namelist():
                if member.lower().endswith((".exe", ".dll")):
                    yield f"{path}:{member}", archive.read(member)
    else:
        with open(path, "rb") as stream:
            yield path, stream.read()


def main(paths):
    if not paths:
        sys.exit(__doc__)
    bad = checked = 0
    for path in paths:
        for name, data in binaries(path):
            checked += 1
            for item in newer_than_win7(pe_imports(data)):
                print(f"{name}: imports {item}, which Windows 7 does not have")
                bad += 1
    if not checked:
        sys.exit("check_win7_imports: no .exe or .dll to check")
    if bad:
        sys.exit(1)
    print(f"check_win7_imports: {checked} file(s), nothing newer than Windows 7")


if __name__ == "__main__":
    main(sys.argv[1:])

#!/usr/bin/env python3
"""Compare the native and translated software GTE on guest addresses and a
seeded run of commands."""

import argparse
import os
import subprocess

from guest_build import darwin_flags
from guest_test_compile import guest_object
from llvm_guest import ROOT, toolchain, translate
from macos_deps import sdk_path

GTE = "src/pc/compat/gte.c"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--instrumented", action="store_true")
    parser.add_argument("--differential", action="store_true")
    args = parser.parse_args()
    out = ROOT / "tmp/gte-boundaries"
    out.mkdir(parents=True, exist_ok=True)
    flags = [
        *darwin_flags(sdk_path()),
        "-O2",
        "-std=c11",
        "-DMEMORIES_PC",
        "-DMEMORIES_TRANSLATED",
        "-I" + str(ROOT / "src"),
        "-I" + str(ROOT / "src/pc/compat"),
    ]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    native = ROOT / GTE
    sources = [native]
    if args.instrumented or args.differential:
        frontend = [flag for flag in flags if not flag.startswith("-fsanitize=")]
        raw = out / "gte.raw.ll"
        subprocess.run(
            [
                str(toolchain() / "bin/clang"),
                *frontend,
                "-O0",
                "-DMEMORIES_INSTRUMENT_SOFTGPU",
                "-S",
                "-emit-llvm",
                str(native),
                "-o",
                str(raw),
            ],
            check=True,
            timeout=60,
        )
        instrumented = out / "gte.instrumented.ll"
        instrumented.write_text(translate(raw.read_text()))
        sources = [native, instrumented] if args.differential else [instrumented]
    compiler = os.environ.get("CC") or (
        "/usr/bin/clang" if args.sanitize else str(toolchain() / "bin/clang")
    )
    outputs = []
    for source in sources:
        compiled = (
            guest_object(source, out / "gte.o", flags)
            if source.suffix == ".ll"
            else str(source)
        )
        output = []
        for test, name in (("gte_boundary_test.c", "boundaries"), ("gte_test.c", "known-answers")):
            binary = out / name
            command = [
                compiler,
                *flags,
                compiled,
                str(ROOT / "tests/pc" / test),
                str(ROOT / "src/pc/compat/pgxp.c"),
                str(ROOT / "src/pc/memory.c"),
                str(ROOT / "src/pc/guest/translated_runtime.c"),
                "-o",
                str(binary),
            ]
            subprocess.run(command, check=True, timeout=60)
            result = subprocess.run(
                [str(binary)], check=True, capture_output=True, text=True, timeout=30
            )
            print(result.stdout, end="")
            output.append(result.stdout)
        outputs.append(output)
    if args.differential:
        assert outputs[0] == outputs[1], "Native and instrumented GTE outputs differ"
        print("Native/instrumented GTE register hashes and known answers match")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Regression tests for dummydir.

Each case here reproduces a bug that was found and fixed, so that if the fix is
ever undone the failure shows up by name rather than as a mysterious change in
behaviour. Every case is written to fail against the code as it was before its
fix, not merely to pass against the code as it is now.

Usage:  python regression_test.py <path-to-dummydir-executable>
Exit:   0 all cases passed, 1 otherwise.
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

MINIMUM_FILE_SIZE = 15          # DDOperation::MINIMUM_FILE_SIZE
MARKER = b"DummyDir_v1"         # the prefix dummydir writes into everything it creates

results = []


# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------

def run(exe, directory, args):
    """Run one operation. args[0] is the operation, the rest are flags."""
    command = [str(exe), args[0], str(directory)] + args[1:]
    proc = subprocess.run(command, capture_output=True, text=True)
    return proc.returncode, proc.stdout + proc.stderr


def run_raw(exe, argv):
    """Run with complete control over argument order, for testing the parser."""
    proc = subprocess.run([str(exe)] + argv, capture_output=True, text=True)
    return proc.returncode, proc.stdout + proc.stderr


def manifest(directory):
    return Path(directory) / "DummyDir.manifest"


def manifest_entries(directory):
    """Every non-comment line of the manifest."""
    path = manifest(directory)
    if not path.is_file():
        return []
    return [ln for ln in path.read_text(encoding="utf-8").splitlines()
            if ln and not ln.startswith("#")]


def manifest_file_count(directory):
    return sum(1 for ln in manifest_entries(directory) if not ln.endswith(" directory"))


def disk_files(directory):
    return [p for p in Path(directory).rglob("DD_*") if p.is_file()]


def dd_subdirs(directory):
    return [p for p in Path(directory).rglob("DD_*") if p.is_dir()]


def case(fn):
    """Register a test case. Each gets its own throwaway directory."""
    def wrapper(exe):
        directory = Path(tempfile.mkdtemp(prefix="dd_reg_"))
        target = directory / "dir"
        try:
            problem = fn(exe, target)
        except Exception as exc:                      # a crash is a failure, not a stack trace
            problem = f"raised {type(exc).__name__}: {exc}"
        finally:
            shutil.rmtree(directory, ignore_errors=True)
        name = fn.__name__.replace("test_", "").replace("_", " ")
        results.append((name, problem))
        print(f"  {'PASS' if problem is None else 'FAIL'}  {name}")
        if problem is not None:
            for line in str(problem).rstrip().splitlines():
                print(f"          {line}")
    wrapper.is_case = True
    return wrapper


# --------------------------------------------------------------------------
# cases - each returns None to pass, or a string describing what went wrong
# --------------------------------------------------------------------------

@case
def test_failed_writes_are_reported_and_left_off_the_manifest(exe, d):
    """add used to report success for files it could not write.

    The stream state was never checked, so a full disk (or, as staged here, a
    directory that disappeared) produced "Processing complete!", exit 0, and a
    manifest full of sizes and hashes for files that were never created.
    """
    run(exe, d, ["dadd", "--count=3", "--maxdepth=1", "--seed=1"])
    for sub in dd_subdirs(d):
        shutil.rmtree(sub)                            # manifest still lists them
    code, out = run(exe, d, ["add", "--count=30", "--filesize=300", "--seed=2"])

    if code == 0:
        return "add exited 0 even though most files could not be created"
    if "not on the file system" not in out:
        return f"no mention of the files that could not be created:\n{out}"
    recorded, present = manifest_file_count(d), len(disk_files(d))
    if recorded != present:
        return f"manifest lists {recorded} files but {present} exist on disk"
    return None


@case
def test_rebuild_rejects_files_too_short_to_be_ours(exe, d):
    """rebuild used to adopt any DD_ file shorter than the marker.

    The length test was a precondition for the content test rather than part of
    it, so a short file skipped validation entirely, was added to the manifest,
    and was then deleted by the next clean - which trusts the manifest.
    """
    run(exe, d, ["add", "--count=3", "--filesize=500", "--seed=1"])
    decoy = d / "DD_short.bin"
    decoy.write_bytes(b"XYZ")                         # shorter than the marker
    manifest(d).unlink()
    run(exe, d, ["rebuild"])

    if any("DD_short.bin" in ln for ln in manifest_entries(d)):
        return "rebuild adopted a file too short to contain the marker"
    run(exe, d, ["clean"])
    if not decoy.is_file():
        return "clean deleted a file that dummydir never created"
    return None


@case
def test_truncate_does_not_underflow(exe, d):
    """modify --modifytype=truncate underflowed when asked to remove more than a file held.

    file.size() - size wrapped around on unsigned arithmetic, sailing past the
    minimum-size guard and asking resize_file for roughly eighteen exabytes.
    """
    run(exe, d, ["add", "--count=5", "--filesize=1k", "--filetype=binary", "--seed=1"])
    code, out = run(exe, d, ["modify", "--count=100%", "--modifytype=truncate",
                             "--filesize=1M", "--seed=2"])
    if code != 0:
        return f"truncate failed instead of clamping:\n{out}"
    bad = [f"{p.name} is {p.stat().st_size}" for p in disk_files(d)
           if p.stat().st_size != MINIMUM_FILE_SIZE]
    if bad:
        return "files were not clamped to the minimum size: " + ", ".join(bad)
    code, out = run(exe, d, ["verify"])
    if code != 0:
        return f"verify failed after truncate:\n{out}"
    return None


@case
def test_directory_failures_are_reported(exe, d):
    """Directory-level errors were recorded but never shown.

    main only ever walked the file list, so ddelete could destroy the files
    inside a directory, fail to remove the directory itself, and still report
    "Processing complete!" with exit 0.
    """
    run(exe, d, ["dadd", "--count=4", "--maxdepth=1", "--seed=1"])
    run(exe, d, ["add", "--count=20", "--filesize=200", "--seed=2"])
    blocked = 0
    for sub in dd_subdirs(d):
        (sub / "not_ours.txt").write_text("user data", encoding="utf-8")
        blocked += 1
    if blocked == 0:
        return "test setup produced no subdirectories to block"

    code, out = run(exe, d, ["ddelete", "--count=100%", "--seed=3"])
    if code == 0:
        return "ddelete exited 0 despite being unable to remove any directory"
    if "directories could not be processed" not in out:
        return f"the directory failures were not reported:\n{out}"
    return None


@case
def test_verbose_shorthand_in_any_position(exe, d):
    """-v used to swallow whatever argument followed it.

    Only the literal string "--verbose" was treated as taking no value, so "-v"
    consumed the next argument and the shorthand appeared to work only when it
    happened to be last.
    """
    run(exe, d, ["add", "--count=3", "--filesize=1k", "--seed=1"])
    placements = {
        "last":   ["verify", str(d), "-v"],
        "first":  ["-v", "verify", str(d)],
        "middle": ["verify", "-v", str(d)],
    }
    for where, argv in placements.items():
        code, out = run_raw(exe, argv)
        if code != 0 or "successfully validated" not in out:
            return f"-v {where} broke the command (exit {code}):\n{out}"
    return None


@case
def test_sparse_file_at_minimum_size(exe, d):
    """A sparse file of exactly the minimum size always errored.

    The hole's start point was clamped up to the minimum while the end stayed at
    size-1, so getFromRange was called with min > max and threw.
    """
    code, out = run(exe, d, ["add", "--count=3", f"--filesize={MINIMUM_FILE_SIZE}",
                             "--filetype=sparse", "--seed=1"])
    if code != 0 or "could not be processed" in out:
        return f"sparse file at the minimum size failed:\n{out}"
    code, out = run(exe, d, ["verify"])
    if code != 0:
        return f"verify failed on a minimum size sparse file:\n{out}"
    return None


@case
def test_oversized_value_is_rejected(exe, d):
    """--size=1000E silently overflowed instead of being rejected.

    Only the percentage path range-checked its result, so the multiplier path
    cast an out-of-range long double and the operation quietly did nothing.
    """
    code, out = run(exe, d, ["add", "--size=1000E", "--filesize=1k", "--seed=1"])
    if code == 0:
        return f"a value far larger than 64 bits was accepted:\n{out}"
    if "Critical Error" in out:
        return f"rejected by an uncaught exception rather than validation:\n{out}"
    return None


@case
def test_reversed_range_is_rejected_cleanly(exe, d):
    """Ranges were validated with convertStringToNumber, which cannot split them.

    "10m-1k" parsed as the leading number and passed validation, then threw at
    run time and surfaced as a generic critical error.
    """
    code, out = run(exe, d, ["add", "--size=10m-1k", "--filesize=1k", "--seed=1"])
    if code == 0:
        return f"a reversed range was accepted:\n{out}"
    if "Critical Error" in out:
        return f"reported as an uncaught exception rather than a flag problem:\n{out}"
    return None


@case
def test_every_invalid_flag_is_reported(exe, d):
    """validateFlags assigned instead of appending, so only the last error survived."""
    code, out = run(exe, d, ["add", "--notaflag=1", "--alsonotaflag=2"])
    if code == 0:
        return "invalid flags were accepted"
    missing = [f for f in ("notaflag", "alsonotaflag") if f not in out]
    if missing:
        return f"only some invalid flags were reported, missing {missing}:\n{out}"
    return None


@case
def test_filesize_below_minimum_is_clamped(exe, d):
    """The minimum-size clamp only ran when --size was given.

    With --count alone a tiny --filesize was recorded verbatim while the file on
    disk still carried the marker, so the manifest disagreed with reality.
    """
    # binary only: with a random filetype some of these land on the sparse path,
    # which has its own minimum-size bug, and this case would then fail for a
    # reason that has nothing to do with the clamp it is meant to be checking.
    code, out = run(exe, d, ["add", "--count=5", "--filesize=5",
                             "--filetype=binary", "--seed=1"])
    if code != 0:
        return f"add failed:\n{out}"
    small = [f"{p.name} is {p.stat().st_size}" for p in disk_files(d)
             if p.stat().st_size < MINIMUM_FILE_SIZE]
    if small:
        return "files were written below the minimum size: " + ", ".join(small)
    code, out = run(exe, d, ["verify"])
    if code != 0:
        return f"manifest disagreed with what was written:\n{out}"
    return None


@case
def test_manifest_hash_ignores_comments(exe, d):
    """The hash covered the whole file, including the line recording the version.

    That made the hash change on every release even when the directory being
    described was identical, and made hashes uncomparable across versions.
    """
    run(exe, d, ["dadd", "--count=4", "--maxdepth=2", "--seed=1"])
    run(exe, d, ["add", "--count=20", "--filesize=300-900", "--seed=2"])

    def current_hash():
        _, out = run(exe, d, ["verify"])
        for token in out.split():
            if len(token) == 32 and all(c in "0123456789abcdef" for c in token):
                return token
        return None

    before = current_hash()
    if before is None:
        return "no manifest hash was reported"

    text = manifest(d).read_text(encoding="utf-8").splitlines()
    rewritten = ["# Version 9.9.9.9" if ln.startswith("# Version") else ln for ln in text]
    if rewritten == text:
        return "no version comment found to change"
    manifest(d).write_text("\n".join(rewritten) + "\n", encoding="utf-8", newline="\n")

    if current_hash() != before:
        return "editing only a comment changed the manifest hash"

    # ...but a real change must still register
    entries = manifest(d).read_text(encoding="utf-8").splitlines()
    for i, ln in enumerate(entries):
        if ln and not ln.startswith("#") and not ln.endswith(" directory"):
            parts = ln.split(" ")
            parts[-2] = str(int(parts[-2]) + 1)
            entries[i] = " ".join(parts)
            break
    manifest(d).write_text("\n".join(entries) + "\n", encoding="utf-8", newline="\n")
    if current_hash() == before:
        return "changing a recorded file size did not change the hash"
    return None


# --------------------------------------------------------------------------

def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    exe = Path(sys.argv[1]).resolve()
    if not exe.is_file():
        print(f"executable not found: {exe}")
        return 2

    print("=== regression cases ===")
    for fn in list(globals().values()):
        if callable(fn) and getattr(fn, "is_case", False):
            fn(exe)

    failed = [name for name, problem in results if problem is not None]
    print()
    if failed:
        print(f"FAILED ({len(failed)} of {len(results)}):")
        for name in failed:
            print(f"  - {name}")
        return 1
    print(f"All {len(results)} regression cases passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

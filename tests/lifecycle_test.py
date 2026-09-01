#!/usr/bin/env python3
"""
End-to-end lifecycle test for dummydir.

Runs a full sequence of operations against a throwaway directory with fixed seeds,
then checks the two invariants that matter most:

  1. verify passes  - every file on disk matches what the manifest recorded.
  2. rebuild is a no-op - rebuilding the manifest from what is actually on disk
     reproduces the existing manifest exactly, byte for byte. If the manifest and
     the filesystem have drifted apart in any way (a wrong size, a stale hash, an
     entry for a file that is not there, a file on disk that is not on the
     manifest), the rebuilt manifest differs and this test fails.

Finally it runs the whole sequence a second time in a separate directory to confirm
the same seeds still produce the same result.

Usage:  python lifecycle_test.py <path-to-dummydir-executable>
Exit:   0 all checks passed, 1 otherwise.
"""

import filecmp
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# The operation sequence. Each entry is a list of arguments placed after the
# directory. Seeds are fixed so the run is reproducible.
#
# dadd has to come before add so there are subdirectories for the new files to be
# distributed into, and add has to come before the file operations so that
# rename/move/delete have something to act on rather than silently no-opping.
# modify and clean are not exercised here; either is a one-line addition.
SEQUENCE = [
    ["dadd",    "--count=20", "--maxdepth=3", "--seed=a1"],
    ["add",     "--count=250", "--filesize=200-4k", "--seed=a2"],
    ["rename",  "--count=25%", "--seed=a3"],
    ["move",    "--count=25%", "--seed=a4"],
    ["drename", "--count=20%", "--seed=a5"],
    ["dmove",   "--count=20%", "--maxdepth=3", "--seed=a6"],
    ["delete",  "--count=20%", "--seed=a7"],
    ["ddelete", "--count=20%", "--seed=a8"],
]

HASH_RE = re.compile(r"Manifest hash is ([0-9a-f]{32})")

failures = []


def report(ok, label, detail=""):
    print(f"  {'PASS' if ok else 'FAIL'}  {label}")
    if not ok:
        if detail:
            for line in detail.rstrip().splitlines():
                print(f"          {line}")
        failures.append(label)
    return ok


def run(exe, directory, args):
    """Run one dummydir operation and return (exit_code, combined_output).

    args[0] is the operation name, which goes before the directory on the command
    line; everything after it are flags, which go after.
    """
    command = [str(exe), args[0], str(directory)] + args[1:]
    proc = subprocess.run(command, capture_output=True, text=True)
    return proc.returncode, proc.stdout + proc.stderr


def manifest_path(directory):
    return Path(directory) / "DummyDir.manifest"


def manifest_counts(directory):
    """Count directories and files recorded in the manifest, ignoring comments.

    Counted from the file itself rather than parsed out of the program's output,
    because that output formats numbers with the user's locale (thousands
    separators) and would not parse the same way everywhere.
    """
    dirs = files = 0
    path = manifest_path(directory)
    if not path.is_file():
        return 0, 0
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        if line.endswith(" directory"):
            dirs += 1
        else:
            files += 1
    return dirs, files


def last_hash(output):
    found = HASH_RE.findall(output)
    return found[-1] if found else None


def on_disk_counts(directory):
    """Count the DD_ items actually present on the filesystem."""
    root = Path(directory)
    dirs = sum(1 for p in root.rglob("DD_*") if p.is_dir())
    files = sum(1 for p in root.rglob("DD_*") if p.is_file())
    return dirs, files


def describe_failure(code, out):
    """Explain a non-zero exit, including the case where nothing ran at all."""
    if out.strip():
        return out
    # No output whatsoever usually means the process never got as far as main():
    # a missing runtime DLL, an architecture mismatch, or a broken build.
    return (f"exit code {code} ({code & 0xFFFFFFFF:#010x}) with no output at all - "
            f"the executable probably failed to start rather than failing a check")


def run_sequence(exe, directory, label):
    """Run the whole operation sequence.

    Returns the final manifest hash, or None if a step failed. Stops at the first
    failure: once one operation has not done its job the rest are acting on a tree
    that is not what the sequence assumes, so their results say nothing useful and
    reporting them as separate failures just buries the one that matters.
    """
    print(f"\n{label}")
    final_hash = None
    for args in SEQUENCE:
        code, out = run(exe, directory, args)
        if not report(code == 0, f"{args[0]:<8} exit 0", describe_failure(code, out)):
            print("          (stopping the sequence here)")
            return None
        h = last_hash(out)
        if h:
            final_hash = h
    return final_hash


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    exe = Path(sys.argv[1]).resolve()
    if not exe.is_file():
        print(f"executable not found: {exe}")
        return 2

    workspace = Path(tempfile.mkdtemp(prefix="dummydir_test_"))
    try:
        target = workspace / "run1"

        chain_hash = run_sequence(exe, target, "=== operation sequence ===")
        if chain_hash is None:
            print("\nThe operation sequence did not complete, so the manifest checks "
                  "below would only report noise. Stopping here.")
            print(f"\nFAILED ({len(failures)} check{'s' if len(failures) != 1 else ''}):")
            for f in failures:
                print(f"  - {f}")
            return 1

        print("\n=== manifest agrees with the filesystem ===")
        m_dirs, m_files = manifest_counts(target)
        d_dirs, d_files = on_disk_counts(target)
        report(m_dirs == d_dirs, f"directory count matches disk ({m_dirs} manifest / {d_dirs} disk)")
        report(m_files == d_files, f"file count matches disk ({m_files} manifest / {d_files} disk)")
        report(chain_hash is not None, "operations reported a manifest hash")

        print("\n=== verify ===")
        code, out = run(exe, target, ["verify"])
        report(code == 0, "verify exit 0", out if code != 0 else "")
        report("All files were successfully validated!" in out,
               "verify reports every file valid", out)
        report(last_hash(out) == chain_hash,
               f"verify reports the same hash ({chain_hash})",
               f"verify said {last_hash(out)}")
        report(manifest_counts(target) == (m_dirs, m_files),
               "verify left the manifest untouched")

        print("\n=== rebuild reproduces the manifest exactly ===")
        before = workspace / "manifest_before_rebuild"
        shutil.copy(manifest_path(target), before)

        code, out = run(exe, target, ["rebuild"])
        report(code == 0, "rebuild exit 0", out if code != 0 else "")

        r_dirs, r_files = manifest_counts(target)
        report(r_dirs == m_dirs, f"directory count unchanged ({m_dirs} -> {r_dirs})")
        report(r_files == m_files, f"file count unchanged ({m_files} -> {r_files})")
        report(last_hash(out) == chain_hash,
               f"rebuilt manifest has the same hash ({chain_hash})",
               f"rebuild said {last_hash(out)}")

        identical = filecmp.cmp(before, manifest_path(target), shallow=False)
        detail = ""
        if not identical:
            import difflib
            a = before.read_text(encoding="utf-8").splitlines()
            b = manifest_path(target).read_text(encoding="utf-8").splitlines()
            detail = "\n".join(list(difflib.unified_diff(
                a, b, "before rebuild", "after rebuild", lineterm=""))[:20])
        report(identical, "rebuilt manifest is byte-identical", detail)

        print("\n=== same seeds reproduce the same result ===")
        second = workspace / "run2"
        repeat_hash = run_sequence(exe, second, "  (second run)")
        report(repeat_hash == chain_hash,
               "second run produced the same hash",
               f"first {chain_hash} / second {repeat_hash}")

        print()
        if failures:
            print(f"FAILED ({len(failures)} check{'s' if len(failures) != 1 else ''}):")
            for f in failures:
                print(f"  - {f}")
            return 1
        print("All checks passed.")
        return 0
    finally:
        shutil.rmtree(workspace, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())

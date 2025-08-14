#!/usr/bin/env python3
import sys
import re
import argparse
import subprocess
from typing import Tuple

re_verify = re.compile(r"^verify: checked=(\d+), mismatches=(\d+), missing=(\d+)")
re_status = re.compile(r"^status:\s+(OK|FAIL)")
re_mismatch = re.compile(r"^MISSMATCH")
re_error = re.compile(r"^BMSSP_ERROR:\s*(.*)")

summary_keys = ["checked", "mismatches", "missing", "mismatch_lines", "status", "has_error_line"]

def parse_output(lines):
    checked = mismatches = missing = 0
    status = None
    mismatch_lines = 0
    has_error_line = False
    err_msgs = []

    for line in lines:
        line = line.rstrip("\n")
        m = re_verify.match(line)
        if m:
            checked = int(m.group(1))
            mismatches = int(m.group(2))
            missing = int(m.group(3))
            continue
        m = re_status.match(line)
        if m:
            status = m.group(1)
            continue
        if re_mismatch.match(line):
            mismatch_lines += 1
            continue
        m = re_error.match(line)
        if m:
            has_error_line = True
            err_msgs.append(m.group(1))
            continue

    return {
        "checked": checked,
        "mismatches": mismatches,
        "missing": missing,
        "mismatch_lines": mismatch_lines,
        "status": status,
        "has_error_line": has_error_line,
        "error_msgs": err_msgs,
    }


def run_and_capture(cmd: list) -> Tuple[int, str]:
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out, err = p.communicate()
    return p.returncode, out


def main():
    ap = argparse.ArgumentParser(description="Validate existing compare_bmssp output without modifying code.")
    ap.add_argument('-f', '--file', help='read output from file instead of running command')
    ap.add_argument('-q', '--quiet', action='store_true', help='quiet mode: only summary and exit code')
    ap.add_argument('-b', '--bin', default='./compare_bmssp', help='path to compare_bmssp binary (default: ./compare_bmssp)')
    ap.add_argument('args', nargs='*', help='arguments to pass to the binary, e.g. n mdeg seed')
    args = ap.parse_args()

    if args.file:
        with open(args.file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    else:
        cmd = [args.bin] + args.args
        # force non-strict to avoid early non-zero exit masking output
        env = dict(**dict(**{k: v for k, v in dict().items()}))
        # Note: run in shell env; here simply run the binary without BMSSP_STRICT
        rc, out = run_and_capture(cmd)
        lines = out.splitlines()

    res = parse_output(lines)

    if not args.quiet:
        print("summary:")
        for k in summary_keys:
            print(f"  {k}={res.get(k)}")
        if res.get('error_msgs'):
            print("  BMSSP_ERROR messages:")
            for msg in res['error_msgs']:
                print(f"    - {msg}")

    # Exit policy: fail if status != OK or mismatches>0 or missing>0 or BMSSP_ERROR present
    fail = (
        (res.get('status') != 'OK') or
        (res.get('mismatches', 0) > 0) or
        (res.get('missing', 0) > 0) or
        res.get('has_error_line', False)
    )
    sys.exit(2 if fail else 0)


if __name__ == '__main__':
    main()

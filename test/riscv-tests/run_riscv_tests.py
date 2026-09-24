#!/usr/bin/env python3
import argparse, contextlib, glob, os, re, subprocess, sys, tempfile

INSN_RE = re.compile(r'^\s*([0-9a-f]+):\s+([0-9a-f]+)\s+(.*)$')
EXIT_RE = re.compile(r'Program exited \(code (\d+)\)')
REG_RE = re.compile(r'^x(\d+):\s+0x([0-9a-fA-F]+)$')

CUSTOM_RISCV_TEST_H = '''\
#ifndef _ENV_PHYSICAL_SINGLE_CORE_H
#define _ENV_PHYSICAL_SINGLE_CORE_H

#define RVTEST_RV32U
#define RVTEST_RV32M

#define RVTEST_CODE_BEGIN \\
    .text; \\
    .align 2; \\
    .globl _start; \\
_start:

#define RVTEST_CODE_END

#define TESTNUM gp

#define RVTEST_PASS \\
    li TESTNUM, 1; \\
    li a0, 0; \\
    li a7, 17; \\
    ecall

#define RVTEST_FAIL \\
    sll TESTNUM, TESTNUM, 1; \\
    or  TESTNUM, TESTNUM, 1; \\
    addi a0, TESTNUM, 0; \\
    li a7, 17; \\
    ecall

#define RVTEST_DATA_BEGIN .align 4;
#define RVTEST_DATA_END .align 4;

#endif
'''

def run_checked(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise subprocess.CalledProcessError(r.returncode, cmd, r.stdout, r.stderr)
    return r

def err_text(e):
    return (e.stderr or "").strip() or str(e)

def write_custom_env(work_dir):
    env_dir = os.path.join(work_dir, "custom_env")
    os.makedirs(env_dir, exist_ok=True)
    with open(os.path.join(env_dir, "riscv_test.h"), "w") as f:
        f.write(CUSTOM_RISCV_TEST_H)
    return env_dir

def preprocess(test_path, riscv_tests_dir, custom_env_dir, ref_prefix, out_path):
    macros_dir = os.path.join(riscv_tests_dir, "isa", "macros", "scalar")
    cmd = [f"{ref_prefix}gcc", "-E", "-P", "-march=rv32im", "-mabi=ilp32",
           "-x", "assembler-with-cpp",
           f"-I{custom_env_dir}", f"-I{macros_dir}", test_path, "-o", out_path]
    run_checked(cmd)

def assemble_mine(my_as, src, out_elf):
    return subprocess.run([my_as, "-o", out_elf, src], capture_output=True, text=True)

def assemble_ref(ref_prefix, src, work_dir, name, base_addr):
    obj = os.path.join(work_dir, name + ".ref.o")
    elf = os.path.join(work_dir, name + ".ref.elf")
    last_err = None
    for march in ("rv32im_zifencei", "rv32im"):
        try:
            run_checked([f"{ref_prefix}as", f"-march={march}", "-mabi=ilp32", "-mno-relax",
                         "-o", obj, src])
            last_err = None
            break
        except subprocess.CalledProcessError as e:
            last_err = e
    if last_err is not None:
        raise last_err
    run_checked([f"{ref_prefix}ld", "-m", "elf32lriscv", "-N", "--no-relax",
                 f"-Ttext={base_addr:#x}", "-e", "_start", "-o", elf, obj])
    return elf

def disassemble(objdump, elf_path):
    result = run_checked([objdump, "-d", "--no-aliases", elf_path])
    insns = []
    for line in result.stdout.splitlines():
        m = INSN_RE.match(line)
        if m:
            addr, enc, rest = m.groups()
            insns.append((addr, enc.strip(), rest.split("#")[0].strip()))
    return insns

def diff_insns(mine, ref):
    mismatches = []
    for i, a_ref in enumerate(ref):
        a_mine = mine[i] if i < len(mine) else None
        if a_mine is None or a_mine[0] != a_ref[0] or a_mine[1] != a_ref[1]:
            mismatches.append((a_ref, a_mine))
    for extra in mine[len(ref):]:
        mismatches.append((None, extra))
    return mismatches

def check_encoding(name, pre, args, work_dir, my_elf):
    try:
        ref_elf = assemble_ref(args.ref_prefix, pre, work_dir, name, args.base_addr)
    except subprocess.CalledProcessError as e:
        return "REF_TOOLCHAIN_ERROR", err_text(e)
    objdump = f"{args.ref_prefix}objdump"
    mine_insns = disassemble(objdump, my_elf)
    ref_insns = disassemble(objdump, ref_elf)
    mismatches = diff_insns(mine_insns, ref_insns)
    return ("PASS", None) if not mismatches else ("MISMATCH", mismatches)

def check_emulator(name, elf_path, args):
    cmd_script = "run\ninfo registers\nquit\n"
    try:
        proc = subprocess.run([args.emulator, "debug", elf_path], input=cmd_script,
                               capture_output=True, text=True, timeout=args.timeout)
    except subprocess.TimeoutExpired:
        return "HANG", None
    except OSError as e:
        return "ERROR", str(e)

    out = proc.stdout
    m = EXIT_RE.search(out)
    if not m:
        return "CRASH", f"rc={proc.returncode}\nstdout: {out.strip()[-1500:]}\nstderr: {proc.stderr.strip()[-500:]}"

    code = int(m.group(1))
    regs = {}
    for line in out.splitlines():
        rm = REG_RE.match(line.strip())
        if rm:
            regs[int(rm.group(1))] = int(rm.group(2), 16)

    if code == 0:
        return "PASS", None
    return "FAIL", {"exit_code": code, "failing_subtest": code >> 1, "gp": regs.get(3)}

def run_one(test_path, args, work_dir, custom_env_dir):
    name = os.path.basename(test_path)
    pre = os.path.join(work_dir, name + ".s")
    try:
        preprocess(test_path, args.riscv_tests, custom_env_dir, args.ref_prefix, pre)
    except subprocess.CalledProcessError as e:
        return name, {"assemble": ("PREPROCESS_ERROR", err_text(e))}

    if args.emulator_only:
        try:
            ref_elf = assemble_ref(args.ref_prefix, pre, work_dir, name, args.base_addr)
        except subprocess.CalledProcessError as e:
            return name, {"assemble": ("REF_TOOLCHAIN_ERROR", err_text(e))}
        return name, {"emulator": check_emulator(name, ref_elf, args)}

    my_elf = os.path.join(work_dir, name + ".my.elf")
    r = assemble_mine(args.my_as, pre, my_elf)
    if r.returncode != 0:
        return name, {"assemble": ("ASSEMBLER_ERROR", r.stderr.strip())}

    results = {}
    if not args.skip_encoding:
        results["encoding"] = check_encoding(name, pre, args, work_dir, my_elf)
    if not args.skip_emulator:
        results["emulator"] = check_emulator(name, my_elf, args)
    return name, results

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--riscv-tests", required=True)
    p.add_argument("--my-as")
    p.add_argument("--emulator", required=True)
    p.add_argument("--ref-prefix", default="riscv64-unknown-elf-")
    p.add_argument("--patterns", nargs="+", default=["rv32ui/*.S", "rv32um/*.S"])
    p.add_argument("--base-addr", type=lambda x: int(x, 0), default=0x10000)
    p.add_argument("--limit", type=int, default=None)
    p.add_argument("--timeout", type=float, default=5.0)
    p.add_argument("--skip-encoding", action="store_true")
    p.add_argument("--skip-emulator", action="store_true")
    p.add_argument("--emulator-only", action="store_true")
    p.add_argument("--work-dir")
    args = p.parse_args()

    if not args.emulator_only and not args.my_as:
        p.error("--my-as is required unless --emulator-only is given")
    if args.emulator_only:
        args.skip_encoding = True

    isa_dir = os.path.join(args.riscv_tests, "isa")
    test_files = []
    for pat in args.patterns:
        test_files += sorted(glob.glob(os.path.join(isa_dir, pat)))
    if args.limit:
        test_files = test_files[:args.limit]
    if not test_files:
        print(f"No test files found under {isa_dir} matching {args.patterns}")
        sys.exit(1)

    summary = {"encoding": {"PASS": 0, "MISMATCH": 0, "ERROR": 0},
               "emulator": {"PASS": 0, "FAIL": 0, "CRASH": 0, "HANG": 0, "ERROR": 0}}
    details = []

    ctx = contextlib.nullcontext(args.work_dir) if args.work_dir else tempfile.TemporaryDirectory()
    with ctx as work_dir:
        os.makedirs(work_dir, exist_ok=True)
        custom_env_dir = write_custom_env(work_dir)
        for tf in test_files:
            name, results = run_one(tf, args, work_dir, custom_env_dir)
            if "assemble" in results:
                status, detail = results["assemble"]
                print(f"ERROR {name}  [{status}] {detail[:120]}")
                if not args.skip_encoding:
                    summary["encoding"]["ERROR"] += 1
                if not args.skip_emulator:
                    summary["emulator"]["ERROR"] += 1
                details.append((name, results))
                continue

            line = name.ljust(28)
            if "encoding" in results:
                status, detail = results["encoding"]
                bucket = status if status in summary["encoding"] else "ERROR"
                summary["encoding"][bucket] += 1
                line += f"  enc:{status}"
            if "emulator" in results:
                status, detail = results["emulator"]
                bucket = status if status in summary["emulator"] else "ERROR"
                summary["emulator"][bucket] += 1
                line += f"  emu:{status}"
                if status == "FAIL":
                    line += f" (subtest {detail['failing_subtest']})"
            print(line)
            details.append((name, results))

    if not args.skip_encoding:
        print(f"\nEncoding: {summary['encoding']}")
    if not args.skip_emulator:
        print(f"Emulator: {summary['emulator']}")

    print("\n--- failures ---")
    for name, results in details:
        enc = results.get("encoding")
        emu = results.get("emulator")
        if enc and enc[0] != "PASS":
            print(f"\n{name} [encoding {enc[0]}]:")
            if enc[0] == "MISMATCH":
                ref, mine = enc[1][0]
                print(f"  ref:  {ref}")
                print(f"  mine: {mine}")
            else:
                print(f"  {str(enc[1])[:200]}")
        if emu and emu[0] != "PASS":
            print(f"\n{name} [emulator {emu[0]}]:")
            print(f"  {str(emu[1])[:2500]}")

    bad = 0
    for kind, counts in summary.items():
        if kind == "encoding" and args.skip_encoding:
            continue
        if kind == "emulator" and args.skip_emulator:
            continue
        bad += sum(v for k, v in counts.items() if k != "PASS")
    sys.exit(1 if bad else 0)

if __name__ == "__main__":
    main()
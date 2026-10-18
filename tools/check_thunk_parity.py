#!/usr/bin/env python3
"""Fail the build if the MASM and GAS spellings of the detour thunks drift.

The project carries two hand-maintained sources for the same ten entry points:
`src/Hooks/*.asm` for MSVC and `src/Hooks/*.s` for everyone else. They encode
an engine call site's exact register and stack contract, so a divergence is not
a style problem -- one side would preserve the wrong state and corrupt the game.

Two things are compared, per thunk:

  * the symbol contract: the entry points each file defines and the C++ symbols
    each file imports;
  * the opcode sequence, and separately the sorted set of stack offsets each
    file touches. The offset set is the sharper test: reordering or resizing a
    spill slot in one file only is exactly the mistake that reads as plausible.

Run directly, or via the cpvr_thunk_parity_check target.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

HOOKS = Path(__file__).resolve().parent.parent / "src" / "Hooks"

# GAS mnemonics carry the operand size that MASM infers from the register, and
# a few MASM spellings differ from their GAS equivalent.
MNEMONIC_ALIASES = {
    "movq": "mov", "movl": "mov", "movw": "mov", "movb": "mov", "mov": "mov",
    "pushq": "push", "popq": "pop", "pushfq": "pushfq", "popfq": "popfq",
    "leaq": "lea", "callq": "call", "jmpq": "jmp",
    "addq": "add", "subq": "sub", "testb": "test", "testq": "test",
    "retq": "ret",
    "jz": "je", "jnz": "jne",
    "movsd": "movsd", "movss": "movss", "movdqu": "movdqu",
}

MASM_DIRECTIVES = {
    "option", "extern", "public", "end", "proc", "endp", "macro", "endm",
    "include", "assume", "align",
}

MACRO_NAMES = {"savevolatile", "restorevolatile", "restoreincoming"}

# Register operands. The two dialects order operands differently (MASM is
# dst,src and GAS is src,dst), so registers are compared as a per-instruction
# set rather than a sequence.
REGISTER = re.compile(r"\b((?:[er]?)(?:[abcd]x|[sd]i|sp|bp)|r\d+|xmm\d+)\b")


def registers(lines: list[str]) -> list[tuple[str, ...]]:
    out = []
    for line in lines:
        found = REGISTER.findall(line.replace("%", "").lower())
        out.append(tuple(sorted(found)))
    return out


def _strip_comment(line: str, markers: str) -> str:
    for m in markers:
        idx = line.find(m)
        if idx >= 0:
            line = line[:idx]
    return line


def masm_lines(path: Path) -> list[str]:
    out = []
    in_block = False
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if in_block:
            if "*/" in line:
                in_block = False
            continue
        if line.startswith("/*"):
            if "*/" not in line:
                in_block = True
            continue
        line = _strip_comment(line, ";").strip()
        if not line:
            continue
        low = line.lower()
        if low.split()[0] in MASM_DIRECTIVES:
            continue
        if low.startswith("."):  # .code .allocstack .pushreg .setframe .endprolog
            continue
        if low.endswith(" proc frame") or low.endswith(" proc"):
            continue
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_$]*:", line):
            continue  # label, e.g. NativeClamp:
        if low.endswith(" macro"):
            continue
        if low.split()[0].rstrip(":") in MACRO_NAMES:
            continue  # macro invocation, not an opcode
        if low.endswith(" endp"):
            continue
        out.append(line)
    return out


def gas_lines(path: Path) -> list[str]:
    out = []
    in_block = False
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if in_block:
            if "*/" in line:
                in_block = False
            continue
        if line.startswith("/*"):
            if "*/" not in line:
                in_block = True
            continue
        line = _strip_comment(line, "#").strip()
        if not line:
            continue
        if line.startswith("."):  # directives: .text .globl .extern .cfi_* .macro
            continue
        if re.fullmatch(r"[A-Za-z_.][A-Za-z0-9_.$]*:", line):
            continue  # label
        if line.lower().lstrip(".").rstrip(":").replace("_", "") in MACRO_NAMES:
            continue  # macro invocation, not an opcode
        out.append(line)
    return out


def mnemonic(line: str) -> str:
    m = re.match(r"^([A-Za-z][A-Za-z0-9]*)\b", line)
    return m.group(1).lower() if m else ""


def _norm_offset(value: int) -> str:
    return f"0x{value:X}" if value >= 0 else f"-0x{-value:X}"


def stack_offsets(lines: list[str]) -> list[str]:
    """Every stack displacement either dialect mentions, normalised to 0xNN."""
    found = set()
    for line in lines:
        # MASM: [rsp+30h]  /  [rsp-50h]      GAS: 0x30(%rsp) / -0x50(%rsp)
        for m in re.finditer(r"\[\s*rsp\s*([+-])\s*([0-9A-Fa-f]+)h?\s*\]", line):
            sign, digits = m.groups()
            found.add(_norm_offset(int(digits, 16) * (-1 if sign == "-" else 1)))
        for m in re.finditer(r"(-?)0[xX]([0-9A-Fa-f]+)\s*\(\s*%rsp\s*\)", line):
            sign, digits = m.groups()
            found.add(_norm_offset(int(digits, 16) * (-1 if sign == "-" else 1)))
    return sorted(found)


def symbols_masm(path: Path) -> tuple[set[str], set[str]]:
    text = path.read_text()
    defined = {m.group(1) for m in re.finditer(r"^(\w+)\s+PROC\b", text, re.M)}
    imported = {m.group(1) for m in re.finditer(r"^EXTERN\s+(\w+)\s*:", text, re.M)}
    return defined, imported


def symbols_gas(path: Path) -> tuple[set[str], set[str]]:
    text = path.read_text()
    defined = {m.group(1) for m in re.finditer(r"^\s*\.globl\s+(\w+)\s*$", text, re.M)}
    imported = {m.group(1) for m in re.finditer(r"^\s*\.extern\s+(\w+)\s*$", text, re.M)}
    return defined, imported


def check(thunk: str) -> list[str]:
    asm_path, s_path = HOOKS / f"{thunk}.asm", HOOKS / f"{thunk}.s"
    problems: list[str] = []

    if not asm_path.exists():
        problems.append(f"{thunk}: missing {asm_path.name}")
    if not s_path.exists():
        problems.append(f"{thunk}: missing {s_path.name}")
    if problems:
        return problems

    adef, aimp = symbols_masm(asm_path)
    sdef, simp = symbols_gas(s_path)
    if adef != sdef:
        problems.append(f"{thunk}: exported symbols differ\n"
                        f"    asm only: {sorted(adef - sdef)}\n"
                        f"    .s  only: {sorted(sdef - adef)}")
    if aimp != simp:
        problems.append(f"{thunk}: imported symbols differ\n"
                        f"    asm only: {sorted(aimp - simp)}\n"
                        f"    .s  only: {sorted(simp - aimp)}")

    alines, slines = masm_lines(asm_path), gas_lines(s_path)
    amn = [MNEMONIC_ALIASES.get(mnemonic(l), mnemonic(l)) for l in alines]
    smn = [MNEMONIC_ALIASES.get(mnemonic(l), mnemonic(l)) for l in slines]
    if amn != smn:
        first = next((i for i, (a, b) in enumerate(zip(amn, smn)) if a != b), None)
        detail = ""
        if first is not None:
            detail = f" first at instruction {first}: " \
                     f"asm={alines[first]!r} .s={slines[first]!r}"
        problems.append(f"{thunk}: opcode sequence differs "
                        f"({len(amn)} asm vs {len(smn)} .s){detail}")

    aoff, soff = stack_offsets(alines), stack_offsets(slines)
    if aoff != soff:
        problems.append(f"{thunk}: stack offsets differ\n"
                        f"    asm only: {[o for o in aoff if o not in soff]}\n"
                        f"    .s  only: {[o for o in soff if o not in aoff]}")

    areg, sreg = registers(alines), registers(slines)
    if areg != sreg:
        idx = next((i for i, (a, b) in enumerate(zip(areg, sreg)) if a != b), None)
        detail = ""
        if idx is not None:
            detail = (f" first at instruction {idx}: "
                      f"asm={alines[idx]!r} .s={slines[idx]!r}")
        problems.append(f"{thunk}: register operands differ{detail}")
    return problems


def main() -> int:
    thunks = sorted(p.stem for p in HOOKS.glob("*.asm"))
    if not thunks:
        print(f"no thunks found under {HOOKS}", file=sys.stderr)
        return 1

    problems: list[str] = []
    for thunk in thunks:
        problems.extend(check(thunk))

    if problems:
        print("thunk parity check FAILED:\n", file=sys.stderr)
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        print("\nThese files implement the same ten entry points. Fix both sides, "
              "not one.", file=sys.stderr)
        return 1

    print(f"thunk parity OK ({len(thunks)} pairs)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
#!/usr/bin/env python3
"""Extract Windows x64 detector names from the OpenRGB 1.1 baseline.

Named detectors are the REGISTER_* entries stored under Detectors.detectors.
I2C bus detectors have no settings name and are omitted on purpose.
"""

from __future__ import annotations

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
CONTROLLERS = ROOT / "Controllers"
OUTPUT = ROOT / "experiment" / "OpenRGB.detectors.template.json"

ENABLED = (
    "Razer Goliathus Extended",
    "MSI Mystic Light MS_7D89",
)

OS_TAGS = ("Windows", "Linux", "FreeBSD", "MacOS")

EXCLUDED_MACROS = {
    "REGISTER_I2C_BUS_DETECTOR",
    "REGISTER_DYNAMIC_DETECTOR",
    "REGISTER_PRE_DETECTION_HOOK",
    "REGISTER_CUSTOM_UDEV_RULE",
    "REGISTER_MANUAL_DEVICE_TYPE",
}

MACRO_RE = re.compile(r"\bREGISTER_[A-Z0-9_]+")
DEFINE_RE = re.compile(r"^[ \t]*#define[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]+\"")
IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def file_is_windows_x64(path: pathlib.Path) -> bool:
    name = path.name
    tags = {tag for tag in OS_TAGS if f"_{tag}" in name}
    if not tags:
        return True
    return "Windows" in tags


def strip_comments(text: str) -> str:
    out = []
    i = 0
    n = len(text)
    while i < n:
        if text.startswith("//", i):
            i = text.find("\n", i)
            if i < 0:
                break
            continue
        if text.startswith("/*", i):
            end = text.find("*/", i + 2)
            if end < 0:
                break
            out.append("\n" * text.count("\n", i, end))
            i = end + 2
            continue
        if text[i] == '"':
            out.append('"')
            i += 1
            while i < n:
                out.append(text[i])
                if text[i] == "\\" and i + 1 < n:
                    out.append(text[i + 1])
                    i += 2
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            continue
        out.append(text[i])
        i += 1
    return "".join(out)


def parse_string_literal(text: str, start: int) -> tuple[str, int]:
    if start >= len(text) or text[start] != '"':
        raise ValueError("expected string literal")
    i = start + 1
    chars = []
    while i < len(text):
        ch = text[i]
        if ch == "\\":
            if i + 1 >= len(text):
                raise ValueError("truncated escape")
            chars.append(text[i + 1])
            i += 2
            continue
        if ch == '"':
            return "".join(chars), i + 1
        chars.append(ch)
        i += 1
    raise ValueError("unterminated string")


def condition_active_for_windows(condition: str) -> bool | None:
    """Return True/False when the condition is obvious, else None (keep)."""
    compact = "".join(condition.split())
    known = {
        "_WIN32": True,
        "WIN32": True,
        "__linux__": False,
        "__APPLE__": False,
        "__FreeBSD__": False,
        "__GLIBC__": False,
        "defined(_WIN32)": True,
        "defined(WIN32)": True,
        "defined(__linux__)": False,
        "defined(__APPLE__)": False,
        "defined(__FreeBSD__)": False,
        "!defined(_WIN32)": False,
        "!defined(WIN32)": False,
        "defined(_WIN32)||defined(__linux__)": True,
        "defined(__linux__)||defined(_WIN32)": True,
        "defined(_WIN32)||defined(_WIN64)": True,
    }
    return known.get(compact)


class Region:
    def __init__(self, active: bool, parent_active: bool, kind: str):
        self.active = active
        self.parent_active = parent_active
        self.kind = kind
        self.taken = active


def directive_active(stack: list[Region], line: str) -> bool:
    stripped = line.strip()
    if stripped.startswith("#"):
        head = stripped[1:].lstrip()
        if head.startswith(("if", "ifdef", "ifndef", "else", "elif", "endif")):
            return True
    return all(region.active for region in stack)


def update_preprocessor(stack: list[Region], line: str, path: pathlib.Path, lineno: int) -> None:
    stripped = line.strip()
    if not stripped.startswith("#"):
        return
    body = stripped[1:].lstrip()
    parent_active = all(region.active for region in stack)

    if body.startswith("ifdef "):
        name = body[6:].strip()
        decision = condition_active_for_windows(name)
        active = parent_active and (True if decision is None else decision)
        stack.append(Region(active, parent_active, "ifdef"))
        return
    if body.startswith("ifndef "):
        name = body[7:].strip()
        decision = condition_active_for_windows(name)
        if decision is None:
            active = parent_active
        else:
            active = parent_active and (not decision)
        stack.append(Region(active, parent_active, "ifndef"))
        return
    if body.startswith("if "):
        expr = body[3:].strip()
        decision = condition_active_for_windows(expr)
        active = parent_active and (True if decision is None else decision)
        stack.append(Region(active, parent_active, "if"))
        return
    if body.startswith("elif "):
        if not stack:
            raise SystemExit(f"{path}:{lineno}: elif without if")
        region = stack[-1]
        expr = body[5:].strip()
        decision = condition_active_for_windows(expr)
        branch = True if decision is None else decision
        region.active = region.parent_active and (not region.taken) and branch
        if region.active:
            region.taken = True
        return
    if body.startswith("else"):
        if not stack:
            raise SystemExit(f"{path}:{lineno}: else without if")
        region = stack[-1]
        region.active = region.parent_active and (not region.taken)
        if region.active:
            region.taken = True
        return
    if body.startswith("endif"):
        if not stack:
            raise SystemExit(f"{path}:{lineno}: endif without if")
        stack.pop()


def macro_is_excluded(macro: str) -> bool:
    if macro in EXCLUDED_MACROS:
        return True
    return macro.startswith((
        "REGISTER_DYNAMIC",
        "REGISTER_CUSTOM",
        "REGISTER_PRE_DETECTION",
        "REGISTER_I2C_BUS",
        "REGISTER_MANUAL",
    ))


def collect_string_defines(text: str) -> dict[str, str]:
    defines = {}
    for line in text.splitlines():
        match = DEFINE_RE.match(line)
        if not match:
            continue
        quote = line.find('"', match.end() - 1)
        name, _ = parse_string_literal(line, quote)
        defines[match.group(1)] = name
    return defines


def extract_file(path: pathlib.Path) -> list[tuple[str, int, str]]:
    raw_lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    stack: list[Region] = []
    active_lines = []
    for lineno, line in enumerate(raw_lines, start=1):
        if directive_active(stack, line):
            active_lines.append(line)
        else:
            active_lines.append("")
        update_preprocessor(stack, line, path, lineno)
    if stack:
        raise SystemExit(f"{path}: unclosed preprocessor conditional")

    text = strip_comments("\n".join(active_lines))
    defines = collect_string_defines(text)
    found = []
    for match in MACRO_RE.finditer(text):
        macro = match.group(0)
        if macro_is_excluded(macro):
            continue
        line_start = text.rfind("\n", 0, match.start()) + 1
        line_end = text.find("\n", match.start())
        if line_end < 0:
            line_end = len(text)
        line = text[line_start:line_end]
        if line.lstrip().startswith("#define"):
            continue
        paren = text.find("(", match.end())
        if paren < 0 or text[match.end():paren].strip() != "":
            continue
        cursor = paren + 1
        while cursor < len(text) and text[cursor].isspace():
            cursor += 1
        if cursor < len(text) and text[cursor] == '"':
            name, _ = parse_string_literal(text, cursor)
        else:
            ident = IDENT_RE.match(text, cursor)
            if ident and ident.group(0) in defines:
                name = defines[ident.group(0)]
            else:
                token = ident.group(0) if ident else text[cursor:cursor + 40]
                raise SystemExit(f"{path}: {macro} name is not a string ({token})")
        lineno = text.count("\n", 0, match.start()) + 1
        found.append((name, lineno, macro))
    return found


def main() -> int:
    records = []
    for path in sorted(CONTROLLERS.rglob("*.cpp")):
        if not file_is_windows_x64(path):
            continue
        for name, lineno, macro in extract_file(path):
            records.append((name, str(path.relative_to(ROOT)), lineno, macro))

    by_name: dict[str, list[tuple[str, int, str]]] = {}
    for name, rel, lineno, macro in records:
        by_name.setdefault(name, []).append((rel, lineno, macro))

    missing = [name for name in ENABLED if name not in by_name]
    if missing:
        raise SystemExit("verified detector names missing from Windows baseline: " + ", ".join(missing))

    if "Faustus" in by_name:
        raise SystemExit("Linux-only Faustus detector was included in the Windows template")
    if "XPG Spectrix S40G" not in by_name:
        raise SystemExit("Windows XPG Spectrix S40G detector is missing from the template")
    if "MSI-RGB" not in by_name:
        raise SystemExit("MSI-RGB detector is missing from the template")

    detectors = {name: name in ENABLED for name in sorted(by_name)}
    document = {
        "Detectors": {
            "hid_safe_mode": True,
            "initial_detection_delay_ms": 0,
            "detectors": detectors,
        }
    }

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(document, indent=4) + "\n", encoding="utf-8")

    enabled = sorted(name for name, value in detectors.items() if value)
    print(f"registrations={len(records)} names={len(detectors)} enabled={enabled}")
    for name in ENABLED:
        for rel, lineno, macro in by_name[name]:
            print(f"enabled-source {name} {rel}:{lineno} {macro}")
    scimitar = by_name.get("Corsair Scimitar Elite RGB", [])
    for rel, lineno, macro in scimitar:
        print(f"scimitar-source {rel}:{lineno} {macro}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

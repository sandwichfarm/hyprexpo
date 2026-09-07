#!/usr/bin/env python3
"""Migrate legacy hyprexpo custom keywords to Hyprland 0.55-style config.

The tool updates legacy hyprland.conf snippets where it can do so safely:
- hyprexpo_workspace_method -> plugin { hyprexpo { workspace_method = ... } }
- hyprexpo_gesture -> Lua snippets using hl.plugin.hyprexpo.gesture(...)
"""

from __future__ import annotations

import argparse
import difflib
import re
import shutil
import sys
from pathlib import Path


WORKSPACE_RE = re.compile(r"^(\s*)hyprexpo_workspace_method\s*=\s*(.*?)\s*(#.*)?$")
GESTURE_RE = re.compile(r"^(\s*)hyprexpo_gesture\s*=\s*(.*?)\s*(#.*)?$")
ASSIGN_RE = re.compile(r"^(\s*)workspace_method\s*=\s*(.*?)\s*(#.*)?$")


def strip_value(value: str) -> str:
    return value.split("#", 1)[0].strip()


def split_csv(value: str) -> list[str]:
    return [part.strip() for part in value.split(",") if part.strip()]


def merge_workspace_value(existing: str, additions: list[str]) -> str:
    values = split_csv(strip_value(existing))
    for addition in additions:
        if addition and addition not in values:
            values.append(addition)
    return ", ".join(values)


def find_block(lines: list[str], block_name: str, start: int = 0, end: int | None = None) -> tuple[int, int] | None:
    end = len(lines) if end is None else end
    depth = 0
    block_start: int | None = None

    for idx in range(start, end):
        stripped = lines[idx].strip()
        if block_start is None and re.match(rf"^{re.escape(block_name)}\s*\{{\s*$", stripped):
            block_start = idx
            depth = 1
            continue

        if block_start is None:
            continue

        depth += stripped.count("{")
        depth -= stripped.count("}")
        if depth <= 0:
            return block_start, idx

    return None


def upsert_workspace_method(lines: list[str], entries: list[str]) -> list[str]:
    plugin_block = find_block(lines, "plugin")
    if plugin_block:
        plugin_start, plugin_end = plugin_block
        hyprexpo_block = find_block(lines, "hyprexpo", plugin_start + 1, plugin_end)
        if hyprexpo_block:
            hyp_start, hyp_end = hyprexpo_block
            for idx in range(hyp_start + 1, hyp_end):
                match = ASSIGN_RE.match(lines[idx])
                if not match:
                    continue
                indent, existing, comment = match.groups()
                merged = merge_workspace_value(existing, entries)
                suffix = f" {comment}" if comment else ""
                lines[idx] = f"{indent}workspace_method = {merged}{suffix}\n"
                return lines

            indent = re.match(r"^(\s*)", lines[hyp_start]).group(1) + "    "
            lines.insert(hyp_end, f"{indent}workspace_method = {', '.join(entries)}\n")
            return lines

        indent = re.match(r"^(\s*)", lines[plugin_start]).group(1) + "    "
        lines[plugin_end:plugin_end] = [
            f"{indent}hyprexpo {{\n",
            f"{indent}    workspace_method = {', '.join(entries)}\n",
            f"{indent}}}\n",
        ]
        return lines

    if lines and lines[-1].strip():
        lines.append("\n")
    lines.extend([
        "plugin {\n",
        "    hyprexpo {\n",
        f"        workspace_method = {', '.join(entries)}\n",
        "    }\n",
        "}\n",
    ])
    return lines


def parse_legacy_gesture(value: str) -> tuple[str | None, str | None]:
    tokens = split_csv(value)
    if not tokens:
        return None, "empty hyprexpo_gesture"

    fingers = ""
    direction = ""
    action = "expo"
    mods = ""
    scale = ""
    disable_inhibit = ""

    if ":" in tokens[0]:
        first = [part.strip() for part in tokens[0].split(":")]
        if len(first) == 3 and first[0] in {"swipe", "gesture"}:
            fingers = first[1]
            direction = first[2]
        else:
            return None, f"unsupported gesture prefix: {tokens[0]}"

        if len(tokens) >= 3 and tokens[1] == "hyprexpo:expo":
            action = "expo"
    else:
        if len(tokens) < 3:
            return None, "expected at least fingers, direction, action"
        fingers = tokens[0]
        direction = tokens[1]
        action = tokens[2]

    for token in tokens[2:]:
        if token.startswith("mod:"):
            mods = token[4:].strip()
        elif token.startswith("scale:"):
            scale = token[6:].strip()
        elif token.startswith("disable_inhibit:"):
            disable_inhibit = token[16:].strip()

    if not fingers.isdigit():
        return None, f"invalid fingers: {fingers}"
    if action not in {"expo", "unset"}:
        return None, f"unsupported action: {action}"

    fields = [f"fingers = {int(fingers)}", f'direction = "{direction}"', f'action = "{action}"']
    if mods:
        fields.append(f'mods = "{mods}"')
    if scale:
        fields.append(f"scale = {scale}")
    if disable_inhibit:
        fields.append(f"disable_inhibit = {disable_inhibit.lower()}")

    return f"hl.plugin.hyprexpo.gesture({{ {', '.join(fields)} }})", None


def migrate_lines(lines: list[str]) -> tuple[list[str], list[str], list[str]]:
    workspace_entries: list[str] = []
    lua_snippets: list[str] = []
    warnings: list[str] = []
    output: list[str] = []

    for line_no, line in enumerate(lines, 1):
        workspace = WORKSPACE_RE.match(line)
        if workspace:
            value = strip_value(workspace.group(2))
            if value:
                workspace_entries.append(value)
            continue

        gesture = GESTURE_RE.match(line)
        if gesture:
            snippet, warning = parse_legacy_gesture(strip_value(gesture.group(2)))
            if snippet:
                lua_snippets.append(snippet)
                output.append(f"{gesture.group(1)}# migrated to Lua: {line.lstrip()}")
            else:
                warnings.append(f"line {line_no}: {warning}")
                output.append(f"{gesture.group(1)}# TODO migrate manually: {line.lstrip()}")
            continue

        output.append(line)

    if workspace_entries:
        output = upsert_workspace_method(output, workspace_entries)

    return output, lua_snippets, warnings


def migrate_file(path: Path, write: bool, backup: bool) -> tuple[list[str], list[str]]:
    original = path.read_text().splitlines(keepends=True)
    migrated, lua_snippets, warnings = migrate_lines(original)

    if write and migrated != original:
        if backup:
            shutil.copy2(path, path.with_suffix(path.suffix + ".bak"))
        path.write_text("".join(migrated))
    elif not write:
        diff = difflib.unified_diff(original, migrated, fromfile=str(path), tofile=f"{path} (migrated)")
        sys.stdout.writelines(diff)

    return lua_snippets, warnings


def main() -> int:
    parser = argparse.ArgumentParser(description="Migrate hyprexpo legacy custom keywords.")
    parser.add_argument("configs", nargs="+", type=Path, help="hyprland.conf files to migrate")
    parser.add_argument("--write", action="store_true", help="rewrite files in place")
    parser.add_argument("--no-backup", action="store_true", help="do not create .bak files with --write")
    parser.add_argument("--lua-out", type=Path, help="write generated Lua gesture snippets to this file")
    args = parser.parse_args()

    all_snippets: list[str] = []
    all_warnings: list[str] = []
    for config in args.configs:
        snippets, warnings = migrate_file(config, args.write, not args.no_backup)
        all_snippets.extend(snippets)
        all_warnings.extend(f"{config}: {warning}" for warning in warnings)

    if all_snippets:
        lua_text = "\n".join(all_snippets) + "\n"
        if args.lua_out and args.write:
            if not args.no_backup and args.lua_out.exists():
                shutil.copy2(args.lua_out, args.lua_out.with_suffix(args.lua_out.suffix + ".bak"))
            args.lua_out.write_text(lua_text)
            print(f"wrote Lua gesture snippets to {args.lua_out}", file=sys.stderr)
        else:
            header = "\n# Lua gesture snippets:"
            if args.lua_out:
                header = "\n# Lua gesture snippets (--lua-out ignored without --write):"
            print(header, file=sys.stderr)
            print(lua_text, file=sys.stderr, end="")

    for warning in all_warnings:
        print(f"warning: {warning}", file=sys.stderr)

    return 1 if all_warnings else 0


if __name__ == "__main__":
    raise SystemExit(main())

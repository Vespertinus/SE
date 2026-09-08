#!/usr/bin/env python3
"""
Minimal YAML -> JSON converter for the SE shader yaml dialect
(drop-in replacement for scripts/yaml2json.py when PyYAML is unavailable).

Supported subset (everything resource/shader/*.yaml uses):
    type: VERTEX            # plain scalar
    include:                # sequence of plain scalars
      - shader/x.sesl
    header:                 # sequence of literal block scalars
     - |
        line
    source:
     - |
        line
Output matches `json.dumps(obj, sort_keys=True, indent=4)`.
"""
import json
import sys


def parse(doc):
    root = {}
    current_key = None
    block = None          # (indent, lines) of an active literal block scalar
    seq = None            # active sequence for current_key

    def flush_block():
        nonlocal block
        if block is None:
            return
        lines = block[1]
        # PyYAML auto-detect semantics: strip the indentation of the
        # first non-empty line from every line.
        content_indent = None
        for ln in lines:
            if ln.strip():
                content_indent = len(ln) - len(ln.lstrip(" "))
                break
        if content_indent is None:
            text = "\n"
        else:
            stripped = [(ln[content_indent:] if len(ln) > content_indent else "")
                        for ln in lines]
            text = "\n".join(stripped) + "\n"
        if seq is not None:
            seq.append(text)
        else:
            root[current_key] = text
        block = None

    for raw in sys.stdin.read().splitlines():
        if not raw.strip():
            if block is not None:
                block[1].append("")
            continue

        indent = len(raw) - len(raw.lstrip(" "))
        line = raw.strip()

        # Inside a literal block scalar: keep verbatim while indentation
        # is deeper than the block's base indent.
        if block is not None and indent > block[0]:
            block[1].append(raw[block[0] + 1:] if len(raw) > block[0] + 1 else "")
            continue
        flush_block()

        if line.startswith("- "):
            content = line[2:].strip()
            if content == "|":
                # literal block scalar starts on the following lines
                block = [indent, []]
                if seq is None:
                    seq = []
                    root[current_key] = seq
                continue
            if seq is None:
                seq = []
                root[current_key] = seq
            seq.append(content)
            continue

        if ":" in line and not line.startswith("#"):
            key, _, value = line.partition(":")
            key, value = key.strip(), value.strip()
            seq = None
            current_key = key
            if value:
                root[key] = value
            continue

        raise SystemExit(f"yaml2json_minimal: unsupported line: {raw!r}")

    flush_block()
    return root


sys.stdout.write(json.dumps(parse(sys.stdin), sort_keys=True, indent=4))

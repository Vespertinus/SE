#!/usr/bin/env bash
# Compiles authored AI assets (FlatBuffers-JSON -> binary):
#   resource/ai/**/bb_*.json -> .sebb (Blackboard.fbs)
#   resource/ai/**/bt_*.json -> .sebt (BehaviorTree.fbs)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${SCRIPT_DIR}/.."
FLATC="${FLATC_BIN:-flatc}"

command -v "$FLATC" >/dev/null 2>&1 || { echo "ERROR: flatc not found" >&2; exit 1; }

cd "$ROOT"

shopt -s globstar nullglob
for f in resource/ai/**/*.json; do
    case "$(basename "$f")" in
        bb_*)
            "$FLATC" --binary -o "$(dirname "$f")" misc/Blackboard.fbs "$f"
            echo "sebb <- $f"
            ;;
        bt_*)
            "$FLATC" --binary -o "$(dirname "$f")" misc/BehaviorTree.fbs "$f"
            echo "sebt <- $f"
            ;;
        *)
            echo "skip (prefix must be bb_ or bt_): $f"
            ;;
    esac
done

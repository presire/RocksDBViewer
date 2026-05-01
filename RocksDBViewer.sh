#!/usr/bin/env sh
# RocksDB GUI Viewer launcher
#
# This script invokes the venv's Python relative to its own directory and
# launches RocksDBViewer.py. If an argument is given it is used as the DB path;
# otherwise the bundled sample_rocksdb is opened (when present).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -x "$SCRIPT_DIR/venv/bin/python" ]]; then
    PYTHON="$SCRIPT_DIR/venv/bin/python"
else
    PYTHON="$(command -v python3 || command -v python)"
fi

if [[ -z "${PYTHON:-}" ]]; then
    echo "RocksDBViewer: Python not found. Create a venv or install python3." >&2
    exit 1
fi

cd "$SCRIPT_DIR"

# If an argument is given, use it.
# When no argument is given and sample_rocksdb exists, use it as a default.
# Otherwise launch without a path (pick from the window).
if [[ $# -gt 0 ]]; then
    exec "$PYTHON" "$SCRIPT_DIR/RocksDBViewer.py" "$1"
elif [[ -d "$SCRIPT_DIR/sample_rocksdb" ]]; then
    exec "$PYTHON" "$SCRIPT_DIR/RocksDBViewer.py" "$SCRIPT_DIR/sample_rocksdb"
else
    exec "$PYTHON" "$SCRIPT_DIR/RocksDBViewer.py"
fi

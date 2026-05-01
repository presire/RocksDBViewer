#!/usr/bin/env bash
# RocksDB GUI Viewer — application launcher installation helper
#
# This script:
#   1. Rewrites the Exec/Icon paths in RocksDBViewer.desktop to match the current directory
#   2. Copies the file to ~/.local/share/applications/
#   3. Refreshes the application cache
#   4. Optionally drops a shortcut on ~/Desktop
# After running, the app appears in your application menu / launcher.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/RocksDBViewer.desktop"
DST_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
DST="$DST_DIR/RocksDBViewer.desktop"

if [[ ! -f "$SRC" ]]; then
    echo "Error: $SRC not found" >&2
    exit 1
fi

mkdir -p "$DST_DIR"

# Rewrite Exec / Icon to match the current directory and copy
sed \
    -e "s|^Exec=.*|Exec=$SCRIPT_DIR/RocksDBViewer.sh %f|" \
    -e "s|^Icon=.*|Icon=$SCRIPT_DIR/assets/RocksDBViewer.png|" \
    "$SRC" > "$DST"
chmod +x "$DST"

echo "Installed: $DST"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$DST_DIR" >/dev/null 2>&1 || true
fi

# Ask whether to also place a shortcut on the desktop
if [[ -d "$HOME/Desktop" ]]; then
    read -rp "Also place a shortcut on the desktop? [y/N]: " yn
    if [[ "$yn" =~ ^[Yy]$ ]]; then
        cp "$DST" "$HOME/Desktop/RocksDBViewer.desktop"
        chmod +x "$HOME/Desktop/RocksDBViewer.desktop"
        echo "Placed: $HOME/Desktop/RocksDBViewer.desktop"
        echo "Hint: on KDE, you may need to right-click the file -> Properties -> mark as executable on first use."
    fi
fi

echo
echo "Search for 'RocksDB GUI Viewer' in your application menu / launcher to find it."

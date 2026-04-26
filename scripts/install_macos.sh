#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# install_macos.sh — copy the built Valvra plugins into the standard macOS
# plug-in directories and make them visible to DAWs.
#
# Run from the project root AFTER a successful Release build.
#   cmake --build build -j4
#   ./scripts/install_macos.sh
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ART="$ROOT/build/src/plugin/ValvraPlugin_artefacts/Release"

VST3_SRC="$ART/VST3/Valvra.vst3"
AU_SRC="$ART/AU/Valvra.component"

VST3_DST="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DST="$HOME/Library/Audio/Plug-Ins/Components"

if [[ ! -d "$VST3_SRC" || ! -d "$AU_SRC" ]]; then
    cat <<EOF >&2
error: plugin bundles not found at
  $VST3_SRC
  $AU_SRC

Build first:
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j4 --target ValvraPlugin_VST3 ValvraPlugin_AU
EOF
    exit 1
fi

mkdir -p "$VST3_DST" "$AU_DST"

echo "Installing Valvra.vst3 → $VST3_DST"
rm -rf "$VST3_DST/Valvra.vst3"
cp -R "$VST3_SRC" "$VST3_DST/"

echo "Installing Valvra.component → $AU_DST"
rm -rf "$AU_DST/Valvra.component"
cp -R "$AU_SRC" "$AU_DST/"

# Refresh macOS plugin registries so DAWs see the new binaries.
# AU: force a rescan by touching the component.
touch "$AU_DST/Valvra.component"

cat <<EOF

✓ Installed.

Gatekeeper first-launch note:
  The plugins are self-signed, so macOS will block them on the first
  scan. In your DAW's plugin-manager list, right-click Valvra → 'Open'
  (or clear the quarantine attribute manually):

    xattr -dr com.apple.quarantine "$VST3_DST/Valvra.vst3"
    xattr -dr com.apple.quarantine "$AU_DST/Valvra.component"

Restart your DAW to pick up the plugins.
EOF

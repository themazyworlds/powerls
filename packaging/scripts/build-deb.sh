#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-release"
STAGE_DIR="$ROOT_DIR/.deb-staging"
OUTPUT_DEB="$ROOT_DIR/powerls_1.0-1_amd64.deb"

rm -rf "$STAGE_DIR" "$OUTPUT_DEB"
mkdir -p "$BUILD_DIR" "$STAGE_DIR/DEBIAN" "$STAGE_DIR/usr/bin"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" >/dev/null

install -m 0755 "$BUILD_DIR/powerls" "$STAGE_DIR/usr/bin/powerls"

cat > "$STAGE_DIR/DEBIAN/control" <<'EOF'
Package: powerls
Version: 1.0-1
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Your Name <you@example.com>
Description: A colorful terminal directory explorer written in C++20
EOF

dpkg-deb --build "$STAGE_DIR" "$OUTPUT_DEB"

echo "Built $OUTPUT_DEB"
dpkg-deb --contents "$OUTPUT_DEB"

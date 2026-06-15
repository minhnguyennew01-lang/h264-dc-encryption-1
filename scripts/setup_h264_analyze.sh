#!/bin/bash
# Setup script: Build h264bitstream into local third_party directory
# This ensures h264_analyze persists across reboots (not in /tmp)
# Location: bitstream_impl_arnold2d/scripts/setup_h264_analyze.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALL_DIR="$PROJECT_ROOT/third_party/h264bitstream"

echo "=========================================="
echo "h264_analyze Setup Script"
echo "=========================================="
echo "Project root: $PROJECT_ROOT"
echo "Install directory: $INSTALL_DIR"
echo ""

# Check if already built
if [ -x "$INSTALL_DIR/.libs/h264_analyze" ]; then
    echo "✅ h264_analyze already built at: $INSTALL_DIR/.libs/h264_analyze"
    exit 0
fi

# Create third_party directory if needed
mkdir -p "$PROJECT_ROOT/third_party"

# Clone repository if not already present
if [ ! -d "$INSTALL_DIR" ]; then
    echo "📦 Cloning h264bitstream from GitHub..."
    cd "$PROJECT_ROOT/third_party"
    git clone https://github.com/aizvorski/h264bitstream.git
    echo "✅ Clone complete"
fi

# Build h264bitstream
echo ""
echo "🔨 Building h264bitstream..."
cd "$INSTALL_DIR"

if [ ! -f configure ]; then
    echo "  → Running autoreconf..."
    autoreconf -i 2>&1 | tail -3
fi

if [ ! -f Makefile ]; then
    echo "  → Running configure..."
    ./configure 2>&1 | tail -3
fi

echo "  → Running make..."
make 2>&1 | tail -5

# Verify build
if [ -x ".libs/h264_analyze" ]; then
    echo ""
    echo "✅ Build successful!"
    echo "   Binary: $INSTALL_DIR/.libs/h264_analyze"
    echo "   Library: $INSTALL_DIR/.libs/libh264bitstream.so.0"
else
    echo ""
    echo "❌ Build failed - h264_analyze binary not found"
    exit 1
fi

echo ""
echo "=========================================="
echo "Setup complete. You can now run:"
echo "  cd $PROJECT_ROOT"
echo "  ./extract_nalu_from_h264analyze output.h264"
echo "=========================================="

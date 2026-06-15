#!/bin/bash
# Smart wrapper for extract_nalu_from_h264analyze
# Automatically sets up h264_analyze if needed
# Usage: ./extract_nalu_from_h264analyze_wrapper.sh <input.h264>

if [ $# -ne 1 ]; then
    echo "Usage: $0 <input.h264>"
    exit 1
fi

INPUT_FILE="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Try to find h264_analyze in priority order
H264_ANALYZE=""

# 1. Check local third_party (persistent, preferred)
if [ -x "$SCRIPT_DIR/third_party/h264bitstream/.libs/h264_analyze" ]; then
    H264_ANALYZE="$SCRIPT_DIR/third_party/h264bitstream/.libs/h264_analyze"
    export LD_LIBRARY_PATH="$SCRIPT_DIR/third_party/h264bitstream/.libs:$LD_LIBRARY_PATH"

# 2. Check /usr/local/bin (system-wide install)
elif [ -x "/usr/local/bin/h264_analyze" ]; then
    H264_ANALYZE="/usr/local/bin/h264_analyze"

# 3. Check /tmp (temporary, from previous session)
elif [ -x "/tmp/h264bitstream/.libs/h264_analyze" ]; then
    H264_ANALYZE="/tmp/h264bitstream/.libs/h264_analyze"
    export LD_LIBRARY_PATH="/tmp/h264bitstream/.libs:$LD_LIBRARY_PATH"

# 4. If not found anywhere, run setup script to build it locally
else
    echo "⚙️  h264_analyze not found. Running setup..."
    if [ -x "$SCRIPT_DIR/scripts/setup_h264_analyze.sh" ]; then
        "$SCRIPT_DIR/scripts/setup_h264_analyze.sh"
        if [ -x "$SCRIPT_DIR/third_party/h264bitstream/.libs/h264_analyze" ]; then
            H264_ANALYZE="$SCRIPT_DIR/third_party/h264bitstream/.libs/h264_analyze"
            export LD_LIBRARY_PATH="$SCRIPT_DIR/third_party/h264bitstream/.libs:$LD_LIBRARY_PATH"
        else
            echo "❌ Setup failed - h264_analyze not built"
            exit 1
        fi
    else
        echo "❌ Setup script not found at: $SCRIPT_DIR/scripts/setup_h264_analyze.sh"
        exit 1
    fi
fi

if [ -z "$H264_ANALYZE" ]; then
    echo "❌ h264_analyze not found"
    exit 1
fi

echo "✅ Using h264_analyze: $H264_ANALYZE"
echo ""

# Now run the actual C++ binary (extract_nalu_from_h264analyze_bin)
# But pass h264_analyze path via environment for the binary to use
export H264_ANALYZE_BIN="$H264_ANALYZE"
exec "$SCRIPT_DIR/extract_nalu_from_h264analyze" "$INPUT_FILE"

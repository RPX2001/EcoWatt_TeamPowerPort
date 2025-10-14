#!/bin/bash

# Copy compiled firmware to Flask server directory and update manifest

# Source and destination paths
SRC_PATH=".pio/build/esp32dev/firmware.bin"
DEST_DIR="flask_server/firmware"
DEST_FILE="$DEST_DIR/latest.bin"

# Read version from VERSION file, or use default
if [ -f "VERSION" ]; then
    VERSION=$(cat VERSION | tr -d '\n\r')
else
    VERSION="1.0.2"
fi

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--version)
            VERSION="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-v|--version VERSION]"
            exit 1
            ;;
    esac
done

# Create destination directory if it doesn't exist
mkdir -p "$DEST_DIR"

# Check if source file exists
if [ ! -f "$SRC_PATH" ]; then
    echo "Error: Firmware file not found at $SRC_PATH"
    echo "Please build the project first with: pio run"
    exit 1
fi

# Copy firmware file
cp "$SRC_PATH" "$DEST_FILE"

if [ $? -ne 0 ]; then
    echo "Error: Failed to copy firmware file"
    exit 1
fi

# Calculate SHA256 hash
if command -v shasum >/dev/null 2>&1; then
    SHA256=$(shasum -a 256 "$DEST_FILE" | cut -d' ' -f1)
elif command -v sha256sum >/dev/null 2>&1; then
    SHA256=$(sha256sum "$DEST_FILE" | cut -d' ' -f1)
else
    echo "Error: Neither shasum nor sha256sum found. Cannot calculate hash."
    exit 1
fi

# Get file size
FILE_SIZE=$(stat -f%z "$DEST_FILE" 2>/dev/null || stat -c%s "$DEST_FILE" 2>/dev/null)

# Create manifest JSON
MANIFEST_FILE="$DEST_DIR/manifest.json"
cat > "$MANIFEST_FILE" << EOF
{
  "version": "$VERSION",
  "sha256": "$SHA256",
  "url": "/firmware/latest.bin",
  "size": $FILE_SIZE,
  "updated_at": "$(date -u +"%Y-%m-%dT%H:%M:%S.%3NZ")"
}
EOF

echo "Firmware deployment completed!"
echo "Version: $VERSION"
echo "Source: $SRC_PATH"
echo "Destination: $DEST_FILE"
echo "File size: $(du -h "$DEST_FILE" | cut -f1)"
echo "SHA256: $SHA256"
echo "Manifest: $MANIFEST_FILE"

# Optionally notify Flask server (if running)
SERVER_URL="http://localhost:5001"
if curl -s --connect-timeout 2 "$SERVER_URL" >/dev/null 2>&1; then
    echo ""
    echo "Notifying Flask server of new firmware..."
    RESPONSE=$(curl -s -X POST "$SERVER_URL/update-manifest" \
        -H "Content-Type: application/json" \
        -d "{\"version\": \"$VERSION\"}")
    
    if [ $? -eq 0 ]; then
        echo "Server notified successfully!"
    else
        echo "Failed to notify server (server may not be running)"
    fi
else
    echo ""
    echo "Note: Flask server not running at $SERVER_URL"
    echo "Start it with: cd flask_server && python app.py"
fi

echo ""
echo "ESP32 devices will detect this update on their next check cycle."
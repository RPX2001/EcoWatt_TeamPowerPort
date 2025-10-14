#!/bin/bash

# Update version script - Updates version in both VERSION file and main.cpp

NEW_VERSION=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--version)
            NEW_VERSION="$2"
            shift 2
            ;;
        *)
            echo "Usage: $0 -v|--version NEW_VERSION"
            echo "Example: $0 -v 1.0.3"
            exit 1
            ;;
    esac
done

if [ -z "$NEW_VERSION" ]; then
    echo "Error: Version number required"
    echo "Usage: $0 -v|--version NEW_VERSION"
    echo "Example: $0 -v 1.0.3"
    exit 1
fi

# Validate version format (basic check)
if ! [[ "$NEW_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Version must be in format X.Y.Z (e.g., 1.0.3)"
    exit 1
fi

# Update VERSION file
echo "$NEW_VERSION" > VERSION
echo "✓ Updated VERSION file to: $NEW_VERSION"

# Update main.cpp
sed -i '' "s/const String currentVersion = \"[^\"]*\";/const String currentVersion = \"$NEW_VERSION\";/" src/main.cpp
if [ $? -eq 0 ]; then
    echo "✓ Updated src/main.cpp to: $NEW_VERSION"
else
    echo "✗ Failed to update src/main.cpp"
    exit 1
fi

echo ""
echo "Version updated successfully!"
echo "Next steps:"
echo "1. Build firmware: pio run"
echo "2. Upload initial version: pio run --target upload"
echo "3. Deploy updates: ./copy_firmware.sh"
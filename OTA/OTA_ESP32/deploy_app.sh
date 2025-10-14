#!/bin/bash

# Enhanced deployment script for multiple ESP32 applications
# Supports both automatic building (with PlatformIO) and manual .bin file placement
# Usage: ./deploy_app.sh <app_name> [-v version] [--bin-file path/to/file.bin]
# Example: ./deploy_app.sh blink_1sec -v 2.0.0
# Example: ./deploy_app.sh blink_1sec -v 2.0.0 --bin-file ~/Downloads/blink_1sec.bin

APP_NAME="$1"
APP_VERSION=""
BIN_FILE_PATH=""

# Parse command line arguments
shift # Remove app name from arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--version)
            APP_VERSION="$2"
            shift 2
            ;;
        --bin-file)
            BIN_FILE_PATH="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 <app_name> [-v|--version VERSION] [--bin-file PATH]"
            exit 1
            ;;
    esac
done

# Default applications directory
APPS_DIR="applications"
SRC_DIR="src"
BACKUP_MAIN="src/main_backup.cpp"

# Check if application name provided
if [ -z "$APP_NAME" ]; then
    echo "Usage: $0 <app_name> [-v|--version VERSION] [--bin-file PATH]"
    echo ""
    echo "Examples:"
    echo "  Auto-build: $0 blink_1sec -v 2.0.0"
    echo "  Manual .bin: $0 blink_1sec -v 2.0.0 --bin-file ~/Downloads/blink_1sec.bin"
    echo ""
    echo "Available applications:"
    ls -1 "$APPS_DIR" 2>/dev/null | sed 's/^/  - /'
    exit 1
fi

# Check if application exists
APP_PATH="$APPS_DIR/$APP_NAME"
if [ ! -d "$APP_PATH" ]; then
    echo "Error: Application '$APP_NAME' not found in $APPS_DIR/"
    echo ""
    echo "Available applications:"
    ls -1 "$APPS_DIR" 2>/dev/null | sed 's/^/  - /'
    exit 1
fi

APP_MAIN="$APP_PATH/main.cpp"
if [ ! -f "$APP_MAIN" ]; then
    echo "Error: main.cpp not found in $APP_PATH/"
    exit 1
fi

echo "🚀 Deploying application: $APP_NAME"

# Backup current main.cpp if it exists
if [ -f "$SRC_DIR/main.cpp" ]; then
    echo "📦 Backing up current main.cpp..."
    cp "$SRC_DIR/main.cpp" "$BACKUP_MAIN"
fi

# Copy application main.cpp to src/
echo "📂 Copying application source..."
cp "$APP_MAIN" "$SRC_DIR/main.cpp"

# Update version if provided
if [ -n "$APP_VERSION" ]; then
    echo "🏷️  Updating version to: $APP_VERSION"
    sed -i.bak "s/const String APP_VERSION = \"[^\"]*\"/const String APP_VERSION = \"$APP_VERSION\"/" "$SRC_DIR/main.cpp"
    rm -f "$SRC_DIR/main.cpp.bak"
    
    # Also update VERSION file
    echo "$APP_VERSION" > VERSION
fi

# Handle firmware building or manual .bin file placement
if [ -n "$BIN_FILE_PATH" ]; then
    # Manual .bin file provided
    echo "📁 Using provided .bin file: $BIN_FILE_PATH"
    
    if [ ! -f "$BIN_FILE_PATH" ]; then
        echo "❌ Binary file not found: $BIN_FILE_PATH"
        
        # Restore backup if file not found
        if [ -f "$BACKUP_MAIN" ]; then
            echo "🔄 Restoring backup..."
            cp "$BACKUP_MAIN" "$SRC_DIR/main.cpp"
            rm -f "$BACKUP_MAIN"
        fi
        exit 1
    fi
    
    # Create .pio directory structure if it doesn't exist
    mkdir -p .pio/build/esp32dev
    
    # Copy the provided .bin file to the expected location
    echo "📋 Copying .bin file to build directory..."
    cp "$BIN_FILE_PATH" .pio/build/esp32dev/firmware.bin
    
    echo "✅ Binary file ready for deployment!"
    
else
    # Try to build with PlatformIO
    echo "🔨 Building firmware with PlatformIO..."
    
    # Try different PlatformIO commands
    BUILD_SUCCESS=false
    
    if command -v pio >/dev/null 2>&1; then
        pio run && BUILD_SUCCESS=true
    elif command -v platformio >/dev/null 2>&1; then
        platformio run && BUILD_SUCCESS=true
    elif command -v ~/.platformio/penv/bin/pio >/dev/null 2>&1; then
        ~/.platformio/penv/bin/pio run && BUILD_SUCCESS=true
    else
        echo "❌ PlatformIO not found!"
        echo ""
        echo "💡 Options:"
        echo "1. Install PlatformIO: https://platformio.org/install/cli"
        echo "2. Add to PATH: export PATH=\$PATH:~/.platformio/penv/bin"
        echo "3. Use --bin-file option with pre-compiled .bin file"
        echo ""
        echo "Example with .bin file:"
        echo "./deploy_app.sh $APP_NAME -v $APP_VERSION --bin-file ~/path/to/firmware.bin"
        
        # Restore backup if PlatformIO not found
        if [ -f "$BACKUP_MAIN" ]; then
            echo "🔄 Restoring backup..."
            cp "$BACKUP_MAIN" "$SRC_DIR/main.cpp"
            rm -f "$BACKUP_MAIN"
        fi
        exit 1
    fi
    
    if [ "$BUILD_SUCCESS" = false ]; then
        echo "❌ Build failed!"
        
        # Restore backup if build failed
        if [ -f "$BACKUP_MAIN" ]; then
            echo "🔄 Restoring backup..."
            cp "$BACKUP_MAIN" "$SRC_DIR/main.cpp"
            rm "$BACKUP_MAIN"
        fi
        exit 1
    fi
    
    echo "✅ Build completed successfully!"
fi

# Deploy firmware
echo "📤 Deploying firmware..."
./copy_firmware.sh

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Application '$APP_NAME' deployed successfully!"
    
    # Get version from source file
    DEPLOYED_VERSION=$(grep "const String APP_VERSION" "$SRC_DIR/main.cpp" | sed 's/.*"\([^"]*\)".*/\1/')
    echo "📋 Application: $APP_NAME"
    echo "📋 Version: $DEPLOYED_VERSION"
    echo "📋 Ready for OTA updates!"
    
    # Clean up backup
    rm -f "$BACKUP_MAIN"
else
    echo "❌ Deployment failed!"
    
    # Restore backup on failure
    if [ -f "$BACKUP_MAIN" ]; then
        echo "🔄 Restoring backup..."
        cp "$BACKUP_MAIN" "$SRC_DIR/main.cpp"
        rm "$BACKUP_MAIN"
    fi
    exit 1
fi
# ESP32 EcoWatt Justfile
# Usage: just <command>

# Default recipe (list all commands)
default:
    @just --list

# Build firmware
build:
    cd PIO/ECOWATT && platformio run

# Upload to device
upload:
    cd PIO/ECOWATT && platformio run --target upload

# Clean build artifacts
clean:
    cd PIO/ECOWATT && platformio run --target clean

# Run unit tests
test:
    cd PIO/ECOWATT && platformio test

# Run compression benchmark
test-compression:
    cd PIO/ECOWATT && platformio test --filter compression_test

# Run security tests
test-security:
    cd PIO/ECOWATT && platformio test --filter security_test

# Run fault injection tests
test-fault-injection:
    cd PIO/ECOWATT && platformio test --filter fault_injection_test

# Run protocol tests
test-protocol:
    cd PIO/ECOWATT && platformio test --filter protocol_test

# Build with diagnostics enabled
build-diagnostics:
    cd PIO/ECOWATT && platformio run -D ENABLE_DIAGNOSTICS=1

# Run all tests
test-all:
    cd PIO/ECOWATT && platformio test && echo "✅ All tests passed!"

# Monitor serial output
monitor:
    cd PIO/ECOWATT && platformio device monitor

# Monitor with filter for errors
monitor-errors:
    cd PIO/ECOWATT && platformio device monitor --filter colorize --filter log2file

# Format code (requires clang-format)
format:
    find PIO/ECOWATT/src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
    find PIO/ECOWATT/include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check code style (requires cppcheck)
lint:
    cppcheck PIO/ECOWATT/src --enable=all --suppress=missingIncludeSystem

# Generate documentation (requires doxygen)
docs:
    cd PIO/ECOWATT && doxygen Doxyfile

# Flash and monitor
flash-monitor: upload monitor

# Quick test cycle (build + upload + monitor)
quick: build upload monitor

# Full validation (clean + build + all tests)
validate: clean build test-all
    @echo "✅ Full validation complete!"

# Check dependencies
check-deps:
    @echo "Checking PlatformIO installation..."
    @platformio --version || echo "❌ PlatformIO not installed. Run: pip install platformio"
    @echo "Checking justfile installation..."
    @just --version || echo "❌ just not installed. Visit: https://github.com/casey/just"

# Show build info
info:
    cd PIO/ECOWATT && platformio run --target envdump

# Update dependencies
update-deps:
    cd PIO/ECOWATT && platformio lib update

# List connected devices
list-devices:
    platformio device list

# Build size analysis
size:
    cd PIO/ECOWATT && platformio run --target size

# Erase flash
erase:
    cd PIO/ECOWATT && platformio run --target erase

# OTA update (requires device IP)
ota IP:
    cd PIO/ECOWATT && platformio run --target upload --upload-port {{IP}}

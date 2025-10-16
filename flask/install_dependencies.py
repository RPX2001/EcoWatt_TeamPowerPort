#!/usr/bin/env python3
"""
Quick Installation Script for Flask Server Security Layer

This script installs all required dependencies for the EcoWatt Flask server
with security layer support.

Run: python install_dependencies.py
"""

import subprocess
import sys

def install_package(package):
    """Install a package using pip"""
    try:
        print(f"Installing {package}...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", package])
        print(f"✓ {package} installed successfully")
        return True
    except subprocess.CalledProcessError as e:
        print(f"✗ Failed to install {package}: {e}")
        return False

def main():
    print("=" * 70)
    print("EcoWatt Flask Server - Dependency Installation")
    print("=" * 70)
    print()
    
    packages = [
        "Flask==2.3.3",
        "paho-mqtt==1.6.1",
        "pycryptodome==3.19.0",
        "requests==2.31.0"
    ]
    
    success_count = 0
    total_count = len(packages)
    
    for package in packages:
        if install_package(package):
            success_count += 1
        print()
    
    print("=" * 70)
    print(f"Installation Summary: {success_count}/{total_count} packages installed")
    print("=" * 70)
    
    if success_count == total_count:
        print("✓ All dependencies installed successfully!")
        print()
        print("Next steps:")
        print("  1. Run: python test_security.py")
        print("  2. Run: python flask_server_hivemq.py")
        print()
        print("For more information, see SETUP_GUIDE.md")
        return 0
    else:
        print("✗ Some packages failed to install")
        print("Try installing manually with: pip install -r requirements.txt")
        return 1

if __name__ == "__main__":
    sys.exit(main())

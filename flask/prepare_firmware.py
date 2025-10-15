#!/usr/bin/env python3
"""
Prepare firmware for OTA update
"""

from firmware_manager import FirmwareManager
import sys

def main():
    # Initialize firmware manager
    fm = FirmwareManager('firmware', 'keys')
    
    # Prepare firmware version 1.0.4
    firmware_file = '/Users/prabathwijethilaka/Sem7/Embedded/EcoWatt_TeamPowerPort/flask/firmware/firmware_v1.0.4.bin'
    version = '1.0.4'
    
    print(f"\n{'='*60}")
    print(f"Preparing firmware: {firmware_file}")
    print(f"Version: {version}")
    print(f"{'='*60}\n")
    
    try:
        success = fm.prepare_firmware(firmware_file, version)
        
        if success:
            print(f"\n{'='*60}")
            print("✅ Firmware prepared successfully!")
            print(f"{'='*60}\n")
            
            # Show manifest
            manifest = fm.get_manifest(version)
            if manifest:
                print("Manifest Details:")
                print(f"  Version: {manifest['version']}")
                print(f"  Original Size: {manifest['original_size']:,} bytes")
                print(f"  Encrypted Size: {manifest['encrypted_size']:,} bytes")
                print(f"  SHA-256: {manifest['sha256_hash']}")
                print(f"  Signature (first 64 chars): {manifest['signature'][:64]}...")
                print(f"  Total Chunks: {manifest['total_chunks']}")
                print(f"  Chunk Size: {manifest['chunk_size']} bytes")
            
            return 0
        else:
            print("\n❌ Failed to prepare firmware")
            return 1
            
    except Exception as e:
        print(f"\n❌ Error preparing firmware: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == '__main__':
    sys.exit(main())

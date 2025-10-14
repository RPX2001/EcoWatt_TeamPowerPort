#!/usr/bin/env python3
"""
EcoWatt FOTA Firmware Manager
Handles firmware preparation, encryption, signing, and chunking for secure OTA updates
"""

import os
import json
import base64
import hashlib
import hmac
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, List

from cryptography.hazmat.primitives import serialization, hashes, padding as crypto_padding
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend


class FirmwareManager:
    """Manages firmware preparation, encryption, signing, and chunking for ESP32 OTA updates."""
    
    def __init__(self, firmware_dir="./firmware", keys_dir="./keys"):
        """
        Initialize FirmwareManager with directory paths and load cryptographic keys.
        
        Args:
            firmware_dir (str): Directory to store prepared firmware files
            keys_dir (str): Directory containing cryptographic keys
        """
        self.firmware_dir = Path(firmware_dir)
        self.keys_dir = Path(keys_dir)
        
        # Create firmware directory if it doesn't exist
        self.firmware_dir.mkdir(exist_ok=True)
        
        # Load cryptographic keys
        self.private_key = self._load_rsa_private_key()
        self.aes_key = self._load_aes_key()
        self.hmac_psk = self._load_hmac_psk()
        
        print(f"✅ FirmwareManager initialized")
        print(f"   📁 Firmware directory: {self.firmware_dir.absolute()}")
        print(f"   🔐 Keys loaded from: {self.keys_dir.absolute()}")
    
    def _load_rsa_private_key(self) -> rsa.RSAPrivateKey:
        """Load RSA private key from PEM file."""
        private_key_path = self.keys_dir / "server_private_key.pem"
        
        if not private_key_path.exists():
            raise FileNotFoundError(f"RSA private key not found: {private_key_path}")
        
        with open(private_key_path, 'rb') as f:
            private_key = serialization.load_pem_private_key(
                f.read(),
                password=None,
                backend=default_backend()
            )
        
        print(f"   🔐 RSA private key loaded")
        return private_key
    
    def _load_aes_key(self) -> bytes:
        """Load AES-256 key from binary file."""
        aes_key_path = self.keys_dir / "aes_firmware_key.bin"
        
        if not aes_key_path.exists():
            raise FileNotFoundError(f"AES key not found: {aes_key_path}")
        
        with open(aes_key_path, 'rb') as f:
            aes_key = f.read()
        
        if len(aes_key) != 32:
            raise ValueError(f"Invalid AES key size: {len(aes_key)} bytes (expected 32)")
        
        print(f"   🔒 AES-256 key loaded")
        return aes_key
    
    def _load_hmac_psk(self) -> str:
        """Load HMAC pre-shared key from keys.h header file."""
        keys_header_path = self.keys_dir / "keys.h"
        
        if not keys_header_path.exists():
            raise FileNotFoundError(f"Keys header not found: {keys_header_path}")
        
        with open(keys_header_path, 'r') as f:
            content = f.read()
        
        # Extract HMAC_PSK from C header
        for line in content.split('\n'):
            if 'HMAC_PSK' in line and '=' in line:
                # Extract string between quotes
                start = line.find('"') + 1
                end = line.rfind('"')
                if start > 0 and end > start:
                    hmac_psk = line[start:end]
                    print(f"   🛡️ HMAC PSK loaded")
                    return hmac_psk
        
        raise ValueError("HMAC_PSK not found in keys.h")
    
    def prepare_firmware(self, bin_file_path: str, version: str) -> Dict:
        """
        Prepare firmware for OTA update: hash, sign, encrypt, and chunk.
        
        Args:
            bin_file_path (str): Path to firmware binary file
            version (str): Firmware version string (e.g., "1.0.3")
            
        Returns:
            Dict: Firmware manifest with metadata
        """
        print(f"\n🚀 Preparing firmware for OTA update")
        print(f"   📄 Binary file: {bin_file_path}")
        print(f"   🔖 Version: {version}")
        
        # Validate input file
        bin_path = Path(bin_file_path)
        if not bin_path.exists():
            raise FileNotFoundError(f"Firmware binary not found: {bin_file_path}")
        
        # Read firmware binary
        print(f"   📖 Reading firmware binary...")
        with open(bin_path, 'rb') as f:
            firmware_data = f.read()
        
        original_size = len(firmware_data)
        print(f"   📏 Original size: {original_size:,} bytes")
        
        # Step 1: Calculate SHA-256 hash
        print(f"   🔍 Calculating SHA-256 hash...")
        sha256_hash = hashlib.sha256(firmware_data).hexdigest()
        print(f"   ✅ Hash: {sha256_hash[:16]}...")
        
        # Step 2: Sign hash with RSA private key
        print(f"   ✍️  Signing with RSA-2048...")
        signature = self._sign_firmware_hash(sha256_hash)
        signature_b64 = base64.b64encode(signature).decode('utf-8')
        print(f"   ✅ Signature generated ({len(signature)} bytes)")
        
        # Step 3: Encrypt firmware with AES-256-CBC
        print(f"   🔐 Encrypting with AES-256-CBC...")
        encrypted_data, iv = self._encrypt_firmware(firmware_data)
        iv_b64 = base64.b64encode(iv).decode('utf-8')
        encrypted_size = len(encrypted_data)
        print(f"   ✅ Encrypted size: {encrypted_size:,} bytes")
        
        # Step 4: Calculate chunking parameters
        chunk_size = 1024  # 1KB chunks as per guideline
        total_chunks = (encrypted_size + chunk_size - 1) // chunk_size  # Ceiling division
        print(f"   📦 Chunking: {total_chunks} chunks of {chunk_size} bytes")
        
        # Step 5: Save encrypted firmware
        encrypted_filename = f"firmware_{version}_encrypted.bin"
        encrypted_path = self.firmware_dir / encrypted_filename
        with open(encrypted_path, 'wb') as f:
            f.write(encrypted_data)
        print(f"   💾 Encrypted firmware saved: {encrypted_filename}")
        
        # Step 6: Create manifest
        manifest = {
            "version": version,
            "original_size": original_size,
            "encrypted_size": encrypted_size,
            "sha256_hash": sha256_hash,
            "signature": signature_b64,
            "iv": iv_b64,
            "chunk_size": chunk_size,
            "total_chunks": total_chunks,
            "timestamp": datetime.now().isoformat()
        }
        
        # Step 7: Save manifest
        manifest_filename = f"firmware_{version}_manifest.json"
        manifest_path = self.firmware_dir / manifest_filename
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)
        print(f"   📋 Manifest saved: {manifest_filename}")
        
        print(f"   ✅ Firmware preparation complete!")
        return manifest
    
    def _sign_firmware_hash(self, sha256_hash: str) -> bytes:
        """Sign firmware hash with RSA private key using PSS padding."""
        hash_bytes = bytes.fromhex(sha256_hash)
        
        signature = self.private_key.sign(
            hash_bytes,
            padding.PSS(
                mgf=padding.MGF1(hashes.SHA256()),
                salt_length=padding.PSS.MAX_LENGTH
            ),
            hashes.SHA256()
        )
        
        return signature
    
    def _encrypt_firmware(self, firmware_data: bytes) -> tuple[bytes, bytes]:
        """Encrypt firmware with AES-256-CBC and return encrypted data + IV."""
        # Generate random IV (16 bytes for AES-CBC)
        iv = os.urandom(16)
        
        # Pad firmware to multiple of 16 bytes (AES block size)
        padder = crypto_padding.PKCS7(128).padder()  # 128 bits = 16 bytes
        padded_data = padder.update(firmware_data)
        padded_data += padder.finalize()
        
        # Encrypt with AES-256-CBC
        cipher = Cipher(
            algorithms.AES(self.aes_key),
            modes.CBC(iv),
            backend=default_backend()
        )
        encryptor = cipher.encryptor()
        encrypted_data = encryptor.update(padded_data) + encryptor.finalize()
        
        return encrypted_data, iv
    
    def get_chunk(self, version: str, chunk_number: int) -> Optional[Dict]:
        """
        Get a specific chunk of encrypted firmware with HMAC authentication.
        
        Args:
            version (str): Firmware version
            chunk_number (int): Chunk number (0-based)
            
        Returns:
            Dict: Chunk data with metadata, or None if not found
        """
        # Load manifest
        manifest = self.get_manifest(version)
        if not manifest:
            return None
        
        # Validate chunk number
        total_chunks = manifest["total_chunks"]
        if chunk_number < 0 or chunk_number >= total_chunks:
            return None
        
        # Load encrypted firmware
        encrypted_filename = f"firmware_{version}_encrypted.bin"
        encrypted_path = self.firmware_dir / encrypted_filename
        
        if not encrypted_path.exists():
            return None
        
        # Read chunk from file
        chunk_size = manifest["chunk_size"]
        offset = chunk_number * chunk_size
        
        with open(encrypted_path, 'rb') as f:
            f.seek(offset)
            chunk_data = f.read(chunk_size)
        
        if not chunk_data:
            return None
        
        # Calculate chunk HMAC for authentication
        chunk_hmac = self._calculate_chunk_hmac(chunk_data, chunk_number)
        
        # Encode chunk data as base64
        chunk_b64 = base64.b64encode(chunk_data).decode('utf-8')
        
        return {
            "chunk_number": chunk_number,
            "data": chunk_b64,
            "size": len(chunk_data),
            "hmac": chunk_hmac,
            "total_chunks": total_chunks
        }
    
    def _calculate_chunk_hmac(self, chunk_data: bytes, chunk_number: int) -> str:
        """Calculate HMAC for chunk authentication (anti-replay protection)."""
        # Combine chunk data with chunk number for HMAC
        hmac_input = chunk_data + str(chunk_number).encode('utf-8')
        
        chunk_hmac = hmac.new(
            self.hmac_psk.encode('utf-8'),
            hmac_input,
            hashlib.sha256
        ).hexdigest()
        
        return chunk_hmac
    
    def get_manifest(self, version: str) -> Optional[Dict]:
        """Load and return manifest for specified version."""
        manifest_filename = f"firmware_{version}_manifest.json"
        manifest_path = self.firmware_dir / manifest_filename
        
        if not manifest_path.exists():
            return None
        
        try:
            with open(manifest_path, 'r') as f:
                return json.load(f)
        except (json.JSONDecodeError, IOError):
            return None
    
    def list_versions(self) -> List[str]:
        """Scan firmware directory and return sorted list of available versions."""
        versions = []
        
        # Find all manifest files
        for manifest_file in self.firmware_dir.glob("firmware_*_manifest.json"):
            # Extract version from filename: firmware_{version}_manifest.json
            filename = manifest_file.name
            if filename.startswith("firmware_") and filename.endswith("_manifest.json"):
                version = filename[9:-14]  # Remove "firmware_" and "_manifest.json"
                versions.append(version)
        
        # Sort versions (latest first) - simple string sort for now
        versions.sort(reverse=True)
        return versions
    
    def get_firmware_info(self, version: str) -> Optional[Dict]:
        """Get comprehensive information about a firmware version."""
        manifest = self.get_manifest(version)
        if not manifest:
            return None
        
        encrypted_filename = f"firmware_{version}_encrypted.bin"
        encrypted_path = self.firmware_dir / encrypted_filename
        
        return {
            "version": version,
            "manifest": manifest,
            "encrypted_file_exists": encrypted_path.exists(),
            "encrypted_file_size": encrypted_path.stat().st_size if encrypted_path.exists() else 0,
            "total_chunks": manifest["total_chunks"],
            "chunk_size": manifest["chunk_size"]
        }


def main():
    """Standalone testing function."""
    import sys
    
    if len(sys.argv) != 3:
        print("Usage: python3 firmware_manager.py <firmware.bin> <version>")
        print("Example: python3 firmware_manager.py ../build/firmware.bin 1.0.3")
        return 1
    
    firmware_path = sys.argv[1]
    version = sys.argv[2]
    
    print("🚀 EcoWatt FOTA Firmware Manager - Standalone Test")
    print("="*60)
    
    try:
        # Initialize firmware manager
        fm = FirmwareManager()
        
        # Prepare firmware
        manifest = fm.prepare_firmware(firmware_path, version)
        
        # Test chunk retrieval
        print(f"\n🔍 Testing chunk retrieval...")
        chunk_0 = fm.get_chunk(version, 0)
        if chunk_0:
            print(f"   ✅ Chunk 0: {chunk_0['size']} bytes, HMAC: {chunk_0['hmac'][:16]}...")
        
        # List versions
        versions = fm.list_versions()
        print(f"\n📋 Available versions: {versions}")
        
        print(f"\n✅ Firmware manager test completed successfully!")
        
    except Exception as e:
        print(f"\n❌ Error: {e}")
        return 1
    
    return 0


if __name__ == "__main__":
    exit(main())
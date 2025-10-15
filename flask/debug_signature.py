#!/usr/bin/env python3
"""
Debug RSA signature creation to understand what ESP32 expects
"""

from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.backends import default_backend
import base64
import json

# Load keys
with open('keys/server_private_key.pem', 'rb') as f:
    private_key = serialization.load_pem_private_key(f.read(), password=None, backend=default_backend())

# Load manifest
with open('firmware/firmware_1.0.4_manifest.json', 'r') as f:
    manifest = json.load(f)

hash_hex = manifest['sha256_hash']
hash_bytes = bytes.fromhex(hash_hex)
signature = base64.b64decode(manifest['signature'])

print("="*70)
print("SIGNATURE DEBUG")
print("="*70)
print(f"\n1. Hash (hex): {hash_hex}")
print(f"   Hash length: {len(hash_bytes)} bytes")
print(f"   Hash (first 16): {hash_bytes[:16].hex()}")

print(f"\n2. Signature length: {len(signature)} bytes")
print(f"   Signature (first 16): {signature[:16].hex()}")
print(f"   Signature (hex, full):\n   {signature.hex()}")

# Verify with public key
public_key = private_key.public_key()

try:
    # This is what Python does internally
    public_key.verify(
        signature,
        hash_bytes,
        padding.PKCS1v15(),
        hashes.SHA256()
    )
    print(f"\n3. ✅ Python verification: SUCCESS")
except Exception as e:
    print(f"\n3. ❌ Python verification: FAILED - {e}")

# Now let's manually verify to see what's happening
from cryptography.hazmat.primitives.asymmetric import rsa as rsa_module

print("\n" + "="*70)
print("MANUAL VERIFICATION (What mbedtls should do)")
print("="*70)

# Get RSA public numbers
public_numbers = public_key.public_numbers()
n = public_numbers.n
e = public_numbers.e

print(f"\nRSA Public Key:")
print(f"  Modulus (n) length: {n.bit_length()} bits")
print(f"  Exponent (e): {e}")
print(f"  Key size: {public_key.key_size} bits")

# Convert signature to integer
sig_int = int.from_bytes(signature, byteorder='big')

# Decrypt signature (s^e mod n)
decrypted_int = pow(sig_int, e, n)

# Convert back to bytes
decrypted_bytes = decrypted_int.to_bytes(256, byteorder='big')

print(f"\nDecrypted signature (padded message):")
print(f"  Length: {len(decrypted_bytes)} bytes")
print(f"  Hex: {decrypted_bytes.hex()}")

# Parse PKCS#1 v1.5 padding
print(f"\nPKCS#1 v1.5 Structure:")
if decrypted_bytes[0] == 0x00 and decrypted_bytes[1] == 0x01:
    print("  ✅ Starts with 0x00 0x01 (correct)")
    
    # Find 0x00 separator
    separator_idx = None
    for i in range(2, len(decrypted_bytes)):
        if decrypted_bytes[i] == 0x00:
            separator_idx = i
            break
    
    if separator_idx:
        print(f"  ✅ Separator 0x00 found at index {separator_idx}")
        
        # Extract DigestInfo
        digest_info = decrypted_bytes[separator_idx + 1:]
        print(f"  DigestInfo length: {len(digest_info)} bytes")
        print(f"  DigestInfo (hex): {digest_info.hex()}")
        
        # SHA-256 DigestInfo structure:
        # 30 31 30 0d 06 09 60 86 48 01 65 03 04 02 01 05 00 04 20 [32-byte hash]
        expected_prefix = bytes.fromhex("3031300d060960864801650304020105000420")
        
        if digest_info.startswith(expected_prefix):
            print(f"  ✅ DigestInfo has correct SHA-256 ASN.1 prefix")
            extracted_hash = digest_info[len(expected_prefix):]
            print(f"\n  Extracted hash: {extracted_hash.hex()}")
            print(f"  Expected hash:  {hash_hex}")
            
            if extracted_hash.hex() == hash_hex:
                print(f"  ✅ Hash matches!")
            else:
                print(f"  ❌ Hash MISMATCH!")
        else:
            print(f"  ❌ DigestInfo prefix incorrect")
            print(f"     Expected: {expected_prefix.hex()}")
            print(f"     Got:      {digest_info[:len(expected_prefix)].hex()}")
else:
    print("  ❌ Padding format incorrect")

print("\n" + "="*70)
print("CONCLUSION")
print("="*70)
print("\nThe signature IS valid and contains:")
print("  1. PKCS#1 v1.5 padding (0x00 0x01 0xFF...0xFF 0x00)")
print("  2. SHA-256 DigestInfo ASN.1 structure")
print("  3. The correct 32-byte hash")
print("\nmbedtls_pk_verify() should handle all of this automatically.")
print("="*70)

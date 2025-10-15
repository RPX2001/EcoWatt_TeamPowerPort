#!/usr/bin/env python3

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.hazmat.primitives import serialization
import base64

# Load the private key
with open('keys/server_private_key.pem', 'rb') as f:
    private_key = serialization.load_pem_private_key(f.read(), password=None)

# Load the public key
with open('keys/server_public_key.pem', 'rb') as f:
    public_key = serialization.load_pem_public_key(f.read())

# Test hash (the one from ESP32 logs)
test_hash_hex = '32578bf6c3a73bf34f663add3018df557e28698a680ac1145278b54b5810af13'
test_hash = bytes.fromhex(test_hash_hex)

print('Testing RSA signature with test hash...')
print(f'Hash: {test_hash_hex}')
print(f'Hash length: {len(test_hash)} bytes')

# Sign the hash
signature = private_key.sign(
    test_hash,
    padding.PKCS1v15(),
    hashes.SHA256()
)

print(f'Signature length: {len(signature)} bytes')
print(f'Signature (hex): {signature.hex()[:64]}...')

# Verify the signature
try:
    public_key.verify(
        signature,
        test_hash,
        padding.PKCS1v15(),
        hashes.SHA256()
    )
    print('✅ Python RSA verification: SUCCESS')
except Exception as e:
    print(f'❌ Python RSA verification: FAILED - {e}')

# Print signature in base64 (what the server sends)
sig_b64 = base64.b64encode(signature).decode()
print(f'Signature (base64): {sig_b64[:64]}...')

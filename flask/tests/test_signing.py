#!/usr/bin/env python3
"""
Test to understand Python's RSA signing behavior
"""

from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.backends import default_backend
import hashlib

# Load private key
with open('keys/server_private_key.pem', 'rb') as f:
    private_key = serialization.load_pem_private_key(f.read(), password=None, backend=default_backend())

# Test data
test_data = b"Hello World"

print("="*70)
print("TEST: Understanding RSA Signing")
print("="*70)

# Method 1: Sign raw data directly
print("\nMethod 1: Sign raw data (Python hashes it)")
sig1 = private_key.sign(test_data, padding.PKCS1v15(), hashes.SHA256())
print(f"  Input: {test_data}")
print(f"  Signature: {sig1[:16].hex()}")

# Method 2: Hash first, then sign the hash bytes (WRONG - Python hashes it AGAIN)
print("\nMethod 2: Hash then sign hash bytes (Python hashes the hash!)")
hash1 = hashlib.sha256(test_data).digest()
sig2 = private_key.sign(hash1, padding.PKCS1v15(), hashes.SHA256())
print(f"  Hash: {hash1.hex()}")
print(f"  Signature: {sig2[:16].hex()}")

# Method 3: Use Prehashed for already-hashed data
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed
print("\nMethod 3: Use Prehashed() for already-hashed data")
sig3 = private_key.sign(hash1, padding.PKCS1v15(), Prehashed(hashes.SHA256()))
print(f"  Hash: {hash1.hex()}")
print(f"  Signature: {sig3[:16].hex()}")

print("\n" + "="*70)
print("COMPARISON")
print("="*70)
print(f"Signature 1 == Signature 3: {sig1 == sig3} (should be True)")
print(f"Signature 1 == Signature 2: {sig1 == sig2} (should be False)")
print("\nConclusion: When signing a hash, use Prehashed() wrapper!")

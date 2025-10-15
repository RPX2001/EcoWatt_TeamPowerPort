from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric import padding
import base64

# Load keys
with open('keys/server_private_key.pem', 'rb') as f:
    private_key = serialization.load_pem_private_key(f.read(), password=None)

public_key = private_key.public_key()

# Hash from manifest
hash_hex = "0006dec671e04be987ae43fb13bb58c42850223af089359c63dbea515b0f9289"
hash_bytes = bytes.fromhex(hash_hex)

# Signature from manifest
with open('firmware/firmware_1.0.4_manifest.json', 'r') as f:
    import json
    manifest = json.load(f)
    signature = base64.b64decode(manifest['signature'])

print(f"Hash: {hash_hex}")
print(f"Signature length: {len(signature)}")
print(f"Signature (first 16): {signature[:16].hex()}")

# Verify
try:
    public_key.verify(
        signature,
        hash_bytes,
        padding.PKCS1v15(),
        hashes.SHA256()
    )
    print("✅ Signature verification SUCCESS (Python)")
except Exception as e:
    print(f"❌ Signature verification FAILED: {e}")

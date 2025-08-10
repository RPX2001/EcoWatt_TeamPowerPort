import asyncio
import random

class CloudStub:
    async def upload(self, batch):
        # emulate network latency
        await asyncio.sleep(0.1)
        # 95% success, 5% fail
        if random.random() < 0.05:
            print("[CloudStub] NACK")
            return False
        print(f"[CloudStub] ACK | received {len(batch)} samples")
        return True
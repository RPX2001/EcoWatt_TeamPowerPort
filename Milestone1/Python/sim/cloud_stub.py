import asyncio
import random

class CloudStub:
    async def upload(self, batch):
        await asyncio.sleep(0.1)
        if random.random() < 0.05:
            print("Upload failed — re-queue for next window")
            return False
        print(f"Received ACK → Idle")  # coordinator also prints this line; harmless duplication if kept
        return True
import asyncio
import random
import time

class InverterSIM:
    async def read(self):
        await asyncio.sleep(0.05)
        if random.random() < 0.02:
            return False, {}
        v = round(random.uniform(210.0, 240.0), 2)
        i = round(random.uniform(0.2, 2.0), 3)
        return True, {"t": time.time(), "voltage": v, "current": i}
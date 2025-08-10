import asyncio
import random

class InverterSIM:
    async def read(self):
        # emulate IO latency
        await asyncio.sleep(0.05)
        # 98% success, 2% transient fail
        if random.random() < 0.02:
            return False, {}
        v = round(random.uniform(210.0, 240.0), 2)
        i = round(random.uniform(0.2, 2.0), 3)
        return True, {"voltage": v, "current": i}
import asyncio
import time

class PeriodicTimer:
    def __init__(self, period: float, name: str = "Timer"):
        self.period = float(period)
        self.name = name

    async def ticks(self):
        # Ttick_*: consumes and re-places the "armed" token implicitly by looping
        next_fire = time.monotonic() + self.period
        while True:
            now = time.monotonic()
            delay = max(0.0, next_fire - now)
            await asyncio.sleep(delay)
            print(f"[Ttick_{self.name}] fired")
            yield None
            next_fire += self.period
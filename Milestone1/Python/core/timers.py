import asyncio
import time
from typing import Callable

class PeriodicTimer:
    def __init__(self, period: float, name: str, on_tick: Callable[[], None]):
        self.period = float(period)
        self.name = name
        self.on_tick = on_tick

    async def run(self):
        next_fire = time.monotonic() + self.period
        while True:
            now = time.monotonic()
            delay = max(0.0, next_fire - now)
            await asyncio.sleep(delay)
            self.on_tick()  # Coordinator prints human messages
            next_fire += self.period
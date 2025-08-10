import time
from typing import Dict, Any
from sim.inverter_sim import InverterSIM
from core.buffer import RingBuffer

class AcquisitionScheduler:
    def __init__(self, sim: InverterSIM, buffer: RingBuffer):
        self.sim = sim
        self.buffer = buffer

    async def poll_once(self):
        # T1_DoPoll {SIM_OK}
        ok, reading = await self.sim.read()
        if not ok:
            print("[T1_DoPoll] SIM_OK=false -> poll skipped")
            return

        sample: Dict[str, Any] = {
            "t": time.time(),
            "voltage": reading["voltage"],
            "current": reading["current"],
            "power": reading["voltage"] * reading["current"],
        }
        print(f"[P3 SampleReady] {sample}")

        # T2_BufferPush {BUF_HAS_SPACE}
        pushed = self.buffer.push(sample)
        if pushed:
            print(f"[T2_BufferPush] P4 size={self.buffer.size}")
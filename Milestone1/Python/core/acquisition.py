from typing import Dict, Any, Tuple
from sim.inverter_sim import InverterSIM

class AcquisitionScheduler:
    def __init__(self, sim: InverterSIM):
        self.sim = sim

    async def poll_once(self) -> Tuple[bool, Dict[str, Any]]:
        ok, reading = await self.sim.read()
        if not ok:
            return False, {}
        sample: Dict[str, Any] = {
            "t": reading["t"],
            "voltage": reading["voltage"],
            "current": reading["current"],
            "power": reading["voltage"] * reading["current"],
        }
        return True, sample
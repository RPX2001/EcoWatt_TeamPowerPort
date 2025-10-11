import asyncio
from core.timers import PeriodicTimer
from core.buffer import RingBuffer
from core.acquisition import AcquisitionScheduler
from core.uploader import Uploader
from core.coordinator import Coordinator, Event
from sim.inverter_sim import InverterSIM
from sim.cloud_stub import CloudStub

POLL_PERIOD_S = 2.0
UPLOAD_PERIOD_S = 15.0
BUFFER_CAPACITY = 256

async def main():
    buffer = RingBuffer(capacity=BUFFER_CAPACITY)
    sim = InverterSIM()
    cloud = CloudStub()
    acq = AcquisitionScheduler(sim=sim)
    upl = Uploader(cloud=cloud)

    q: asyncio.Queue[Event] = asyncio.Queue()
    coord = Coordinator(queue=q, buffer=buffer, acq=acq, upl=upl)

    poll_timer = PeriodicTimer(period=POLL_PERIOD_S, name="Poll",
                               on_tick=lambda: q.put_nowait(Event("POLL_TICK")))
    upload_timer = PeriodicTimer(period=UPLOAD_PERIOD_S, name="Upload",
                                 on_tick=lambda: q.put_nowait(Event("UPLOAD_TICK")))

    print("Idle started | (Re)start Poll Timer | (Re)start Upload Timer")
    tasks = [
        asyncio.create_task(coord.run()),
        asyncio.create_task(poll_timer.run()),
        asyncio.create_task(upload_timer.run()),
    ]
    try:
        await asyncio.gather(*tasks)
    except asyncio.CancelledError:
        pass

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
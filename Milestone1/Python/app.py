import asyncio
from core.timers import PeriodicTimer
from core.buffer import RingBuffer
from core.acquisition import AcquisitionScheduler
from core.uploader import Uploader
from sim.inverter_sim import InverterSIM
from sim.cloud_stub import CloudStub

POLL_PERIOD_S = 2.0           # fast sampling
UPLOAD_PERIOD_S = 15.0        # 15s sim window (represents 15 min in real)
BUFFER_CAPACITY = 256

async def main():
    # Places/Modules
    rb = RingBuffer(capacity=BUFFER_CAPACITY)     # P4 Buffer
    sim = InverterSIM()                           # Inverter stub
    cloud = CloudStub()                           # Cloud stub

    # Transitions mapped to methods
    acq = AcquisitionScheduler(sim=sim, buffer=rb)    # T1/T2
    upl = Uploader(buffer=rb, cloud=cloud)            # T4/T5

    # Timers (P8/P9 + Ttick_Poll / Ttick_Upload)
    poll_timer = PeriodicTimer(period=POLL_PERIOD_S, name="Poll")
    upload_timer = PeriodicTimer(period=UPLOAD_PERIOD_S, name="Upload")

    # Event loops
    async def poll_loop():
        async for _ in poll_timer.ticks():
            await acq.poll_once()   # T1_DoPoll -> P3 -> T2_BufferPush

    async def upload_loop():
        async for _ in upload_timer.ticks():
            await upl.upload_once() # T4_UploadBatch -> T5_AckAndFlush

    print("M0: P0=1, P8=1, P9=1 | Starting scaffold…")
    tasks = [asyncio.create_task(poll_loop()), asyncio.create_task(upload_loop())]
    try:
        await asyncio.gather(*tasks)
    except asyncio.CancelledError:
        pass

if __name__ == "__main__":
    asyncio.run(main())
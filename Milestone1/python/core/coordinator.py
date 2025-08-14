import asyncio
from dataclasses import dataclass
from typing import Dict, Any, List, Tuple
from core.buffer import RingBuffer
from core.acquisition import AcquisitionScheduler
from core.uploader import Uploader

@dataclass(frozen=True)
class Event:
    kind: str  # "POLL_TICK" | "UPLOAD_TICK"

class Coordinator:
    """
    Event-driven controller matching the updated Petri net:
    - Central Idle
    - Explicit Poll Ready / Upload Ready
    - Guards: Not Uploading, Not Polling (no overlap)
    """

    def __init__(self,
                 queue: asyncio.Queue,
                 buffer: RingBuffer,
                 acq: AcquisitionScheduler,
                 upl: Uploader):
        self.q = queue
        self.buffer = buffer
        self.acq = acq
        self.upl = upl
        # places (booleans)
        self.idle = True
        self.polling = False
        self.uploading = False
        self.poll_ready = False
        self.upload_ready = False

    async def run(self):
        while True:
            ev = await self.q.get()
            if ev.kind == "POLL_TICK":
                self.poll_ready = True
                print("[Poll Timer = 2s] tick → Poll Ready")
            elif ev.kind == "UPLOAD_TICK":
                self.upload_ready = True
                print("[Upload Timer = 15s] tick → Upload Ready")
            await self._drain_enabled_transitions()

    async def _drain_enabled_transitions(self):
        made_progress = True
        while made_progress:
            made_progress = False

            # Prefer upload to reduce latency when data exists
            if (self.upload_ready and not self.polling and not self.uploading
                    and self.buffer.not_empty()):
                print("Not Polling → Uploading")
                await self._do_uploading()
                made_progress = True
                continue

            if (self.poll_ready and not self.uploading and not self.polling):
                print("Not Uploading → Polling")
                await self._do_polling()
                made_progress = True
                continue

    async def _do_polling(self):
        self.idle = False
        self.polling = True

        ok, sample = await self.acq.poll_once()
        if not ok:
            print("Poll skipped (device unavailable)")
            self.polling = False
            self.idle = True
            return

        print("Sample Ready", sample)
        pushed = self.buffer.push(sample)
        if pushed:
            print("Buffer Push")
            print(f"Pushed | Buffer size = {self.buffer.size}")
        else:
            print("Buffer full — sample dropped")

        self.poll_ready = False
        self.polling = False
        self.idle = True
        print("Idle started")

    async def _do_uploading(self):
        self.idle = False
        self.uploading = True

        batch: List[Dict[str, Any]] = self.buffer.drain_all()
        print(f"Uploading (sending {len(batch)} samples)")
        ok = await self.upl.upload_once(batch)

        if ok:
            print("Received ACK → Idle")
            self.upload_ready = False
        else:
            print("Upload failed — re-queue for next window")
            for item in batch:
                self.buffer.push(item)
            self.upload_ready = False

        self.uploading = False
        self.idle = True
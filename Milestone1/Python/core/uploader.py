from typing import List, Dict, Any
from core.buffer import RingBuffer
from sim.cloud_stub import CloudStub

class Uploader:
    def __init__(self, buffer: RingBuffer, cloud: CloudStub):
        self.buffer = buffer
        self.cloud = cloud

    async def upload_once(self):
        # Guard BUF_NOT_EMPTY
        if not self.buffer.not_empty():
            print("[Guard BUF_NOT_EMPTY] Buffer empty -> no upload")
            return

        # T4_UploadBatch
        batch: List[Dict[str, Any]] = self.buffer.drain_all()
        print(f"[T4_UploadBatch] sending batch of {len(batch)} samples")
        ok = await self.cloud.upload(batch)

        if ok:
            # T5_AckAndFlush
            print("[T5_AckAndFlush] ACK received -> P7 Acked")
        else:
            # Simple failure path: put samples back (one-shot retry strategy)
            print("[Upload Failed] Re-queueing samples for next window")
            for item in batch:
                self.buffer.push(item)
from typing import List, Dict, Any
from sim.cloud_stub import CloudStub

class Uploader:
    def __init__(self, cloud: CloudStub):
        self.cloud = cloud

    async def upload_once(self, batch: List[Dict[str, Any]]) -> bool:
        return await self.cloud.upload(batch)
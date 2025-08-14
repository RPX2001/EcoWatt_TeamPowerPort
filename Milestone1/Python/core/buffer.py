from collections import deque
from typing import List, Dict, Any

class RingBuffer:
    def __init__(self, capacity: int):
        self.capacity = int(capacity)
        self._q = deque()

    @property
    def size(self) -> int:
        return len(self._q)

    def push(self, sample: Dict[str, Any]) -> bool:
        if len(self._q) >= self.capacity:
            print("Buffer full — sample dropped")
            return False
        self._q.append(sample)
        return True

    def drain_all(self) -> List[Dict[str, Any]]:
        items = list(self._q)
        self._q.clear()
        return items

    def not_empty(self) -> bool:
        return len(self._q) > 0
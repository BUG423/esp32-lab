"""
OpenHarmony LiteOS-M Kernel Implementation for ESP32 Architecture
Subsystem: Kernel Lite (Task Scheduling, Mutex, Semaphore, Queue, SwTmr, MemPool)
"""

import threading
import time
from typing import Any, Callable, Optional

# Constants & Return Codes
LOS_OK = 0
LOS_NOK = 1
LOS_WAIT_FOREVER = 0xFFFFFFFF
LOS_NO_WAIT = 0

# Task Priorities: 0 (Highest) to 31 (Lowest)
LOS_TASK_PRIORITY_HIGHEST = 0
LOS_TASK_PRIORITY_LOWEST = 31
LOS_TASK_DEFAULT_PRIORITY = 20

# Task Status
TASK_STATUS_READY = "READY"
TASK_STATUS_RUNNING = "RUNNING"
TASK_STATUS_SUSPENDED = "SUSPENDED"
TASK_STATUS_BLOCKED = "BLOCKED"
TASK_STATUS_DELETED = "DELETED"

# Software Timer Modes
LOS_SWTMR_MODE_ONCE = 0
LOS_SWTMR_MODE_PERIOD = 1
LOS_SWTMR_MODE_NO_SELFDELETE = 2


class LosTaskCB:
    """LiteOS-M Task Control Block."""
    def __init__(self, task_id: int, name: str, entry: Callable, priority: int = 20, stack_size: int = 4096, arg: Any = None):
        self.task_id = task_id
        self.name = name
        self.entry = entry
        self.priority = max(LOS_TASK_PRIORITY_HIGHEST, min(LOS_TASK_PRIORITY_LOWEST, priority))
        self.base_priority = self.priority
        self.stack_size = stack_size
        self.arg = arg
        self.status = TASK_STATUS_READY
        self.run_count = 0
        self.execution_log: list[str] = []

    def run_step(self) -> Any:
        self.status = TASK_STATUS_RUNNING
        self.run_count += 1
        res = None
        if callable(self.entry):
            try:
                res = self.entry(self.arg) if self.arg is not None else self.entry()
            except Exception as e:
                self.execution_log.append(f"ERROR: {e}")
        self.status = TASK_STATUS_READY
        return res


class LosMuxCB:
    """LiteOS-M Mutex Control Block with Priority Inheritance."""
    def __init__(self, mux_id: int):
        self.mux_id = mux_id
        self.owner_task_id: Optional[int] = None
        self.lock_count: int = 0
        self.wait_tasks: list[int] = []
        self.created = True


class LosSemCB:
    """LiteOS-M Semaphore Control Block."""
    def __init__(self, sem_id: int, init_count: int, max_count: int):
        self.sem_id = sem_id
        self.count = init_count
        self.max_count = max_count
        self.wait_tasks: list[int] = []
        self.created = True


class LosQueueCB:
    """LiteOS-M Queue Control Block."""
    def __init__(self, queue_id: int, queue_len: int, max_msg_size: int):
        self.queue_id = queue_id
        self.queue_len = queue_len
        self.max_msg_size = max_msg_size
        self.messages: list[Any] = []
        self.created = True


class LosSwtmrCB:
    """LiteOS-M Software Timer Control Block."""
    def __init__(self, timer_id: int, interval: int, mode: int, handler: Callable, arg: Any = None):
        self.timer_id = timer_id
        self.interval = interval
        self.mode = mode
        self.handler = handler
        self.arg = arg
        self.state = "STOPPED"
        self.ticks_remaining = interval
        self.trigger_count = 0
        self.created = True


class LosMemPool:
    """LiteOS-M Dynamic Memory Pool."""
    def __init__(self, pool_size: int = 64 * 1024):
        self.pool_size = pool_size
        self.allocated_blocks: dict[int, int] = {}  # ptr_id -> size
        self.next_ptr_id = 0x1000
        self.total_allocated = 0
        self.peak_allocated = 0
        self.initialized = True

    def alloc(self, size: int) -> Optional[int]:
        if size <= 0:
            return None
        aligned_size = (size + 3) & ~3
        if self.total_allocated + aligned_size > self.pool_size:
            return None
        ptr = self.next_ptr_id
        self.next_ptr_id += aligned_size + 16
        self.allocated_blocks[ptr] = aligned_size
        self.total_allocated += aligned_size
        if self.total_allocated > self.peak_allocated:
            self.peak_allocated = self.total_allocated
        return ptr

    def free(self, ptr: int) -> bool:
        if ptr in self.allocated_blocks:
            size = self.allocated_blocks.pop(ptr)
            self.total_allocated -= size
            return True
        return False

    def realloc(self, ptr: int, new_size: int) -> Optional[int]:
        if ptr not in self.allocated_blocks:
            return None
        if new_size <= 0:
            self.free(ptr)
            return None
        old_size = self.allocated_blocks[ptr]
        if new_size == old_size:
            return ptr
        aligned_size = (new_size + 3) & ~3
        diff = aligned_size - old_size
        if self.total_allocated + diff > self.pool_size:
            return None
        self.allocated_blocks.pop(ptr)
        self.total_allocated += diff
        new_ptr = self.next_ptr_id
        self.next_ptr_id += aligned_size + 16
        self.allocated_blocks[new_ptr] = aligned_size
        if self.total_allocated > self.peak_allocated:
            self.peak_allocated = self.total_allocated
        return new_ptr

    def get_info(self) -> dict[str, int]:
        return {
            "total_size": self.pool_size,
            "used_size": self.total_allocated,
            "free_size": self.pool_size - self.total_allocated,
            "peak_size": self.peak_allocated,
            "active_allocations": len(self.allocated_blocks)
        }


class LiteOSMKernel:
    """High-fidelity OpenHarmony LiteOS-M Kernel Engine for ESP32 target."""
    
    def __init__(self):
        self._lock = threading.RLock()
        self.tasks: dict[int, LosTaskCB] = {}
        self.next_task_id = 1
        self.current_task_id: Optional[int] = None
        self.ready_queue: list[int] = []
        
        self.mutexes: dict[int, LosMuxCB] = {}
        self.next_mux_id = 1
        
        self.semaphores: dict[int, LosSemCB] = {}
        self.next_sem_id = 1
        
        self.queues: dict[int, LosQueueCB] = {}
        self.next_queue_id = 1
        
        self.timers: dict[int, LosSwtmrCB] = {}
        self.next_timer_id = 1
        
        self.mempool = LosMemPool(pool_size=128 * 1024)
        self.tick_count = 0
        self.scheduler_history: list[str] = []

    # ================= Task Management =================
    def LOS_TaskCreate(self, name: str, entry: Callable, priority: int = 20, stack_size: int = 4096, arg: Any = None) -> tuple[int, int]:
        with self._lock:
            if priority < LOS_TASK_PRIORITY_HIGHEST or priority > LOS_TASK_PRIORITY_LOWEST:
                return LOS_NOK, 0
            task_id = self.next_task_id
            self.next_task_id += 1
            task = LosTaskCB(task_id, name, entry, priority, stack_size, arg)
            self.tasks[task_id] = task
            self._insert_ready_queue(task_id)
            return LOS_OK, task_id

    def _insert_ready_queue(self, task_id: int) -> None:
        if task_id not in self.ready_queue:
            self.ready_queue.append(task_id)
            # Sort by priority (ascending, 0 is highest)
            self.ready_queue.sort(key=lambda tid: self.tasks[tid].priority)

    def LOS_TaskDelete(self, task_id: int) -> int:
        with self._lock:
            if task_id not in self.tasks:
                return LOS_NOK
            task = self.tasks[task_id]
            task.status = TASK_STATUS_DELETED
            if task_id in self.ready_queue:
                self.ready_queue.remove(task_id)
            if self.current_task_id == task_id:
                self.current_task_id = None
            del self.tasks[task_id]
            return LOS_OK

    def LOS_TaskSuspend(self, task_id: int) -> int:
        with self._lock:
            if task_id not in self.tasks:
                return LOS_NOK
            task = self.tasks[task_id]
            if task.status == TASK_STATUS_SUSPENDED:
                return LOS_NOK
            task.status = TASK_STATUS_SUSPENDED
            if task_id in self.ready_queue:
                self.ready_queue.remove(task_id)
            return LOS_OK

    def LOS_TaskResume(self, task_id: int) -> int:
        with self._lock:
            if task_id not in self.tasks:
                return LOS_NOK
            task = self.tasks[task_id]
            if task.status != TASK_STATUS_SUSPENDED:
                return LOS_NOK
            task.status = TASK_STATUS_READY
            self._insert_ready_queue(task_id)
            return LOS_OK

    def LOS_TaskPriSet(self, task_id: int, priority: int) -> int:
        with self._lock:
            if task_id not in self.tasks:
                return LOS_NOK
            if priority < LOS_TASK_PRIORITY_HIGHEST or priority > LOS_TASK_PRIORITY_LOWEST:
                return LOS_NOK
            task = self.tasks[task_id]
            task.priority = priority
            task.base_priority = priority
            if task_id in self.ready_queue:
                self.ready_queue.sort(key=lambda tid: self.tasks[tid].priority)
            return LOS_OK

    def LOS_TaskPriGet(self, task_id: int) -> int:
        with self._lock:
            if task_id not in self.tasks:
                return -1
            return self.tasks[task_id].priority

    def schedule_next(self) -> Optional[int]:
        with self._lock:
            if not self.ready_queue:
                return None
            next_tid = self.ready_queue.pop(0)
            self.current_task_id = next_tid
            task = self.tasks[next_tid]
            self.scheduler_history.append(f"EXEC:{task.name}(pri={task.priority})")
            task.run_step()
            return next_tid

    # ================= Mutex =================
    def LOS_MuxCreate(self) -> tuple[int, int]:
        with self._lock:
            mux_id = self.next_mux_id
            self.next_mux_id += 1
            self.mutexes[mux_id] = LosMuxCB(mux_id)
            return LOS_OK, mux_id

    def LOS_MuxDelete(self, mux_id: int) -> int:
        with self._lock:
            if mux_id not in self.mutexes:
                return LOS_NOK
            mux = self.mutexes[mux_id]
            if mux.owner_task_id is not None:
                return LOS_NOK  # Cannot delete locked mutex
            del self.mutexes[mux_id]
            return LOS_OK

    def LOS_MuxPend(self, mux_id: int, timeout: int = LOS_WAIT_FOREVER, task_id: Optional[int] = None) -> int:
        with self._lock:
            if mux_id not in self.mutexes:
                return LOS_NOK
            mux = self.mutexes[mux_id]
            current_id = task_id or self.current_task_id or 1
            if mux.owner_task_id is None:
                mux.owner_task_id = current_id
                mux.lock_count = 1
                return LOS_OK
            if mux.owner_task_id == current_id:
                mux.lock_count += 1  # Reentrant support
                return LOS_OK
            if timeout == LOS_NO_WAIT:
                return LOS_NOK
            # Priority inheritance simulation: raise owner's priority if current task is higher
            if current_id in self.tasks and mux.owner_task_id in self.tasks:
                cur_pri = self.tasks[current_id].priority
                owner_pri = self.tasks[mux.owner_task_id].priority
                if cur_pri < owner_pri:
                    self.tasks[mux.owner_task_id].priority = cur_pri
            mux.wait_tasks.append(current_id)
            return LOS_OK

    def LOS_MuxPost(self, mux_id: int, task_id: Optional[int] = None) -> int:
        with self._lock:
            if mux_id not in self.mutexes:
                return LOS_NOK
            mux = self.mutexes[mux_id]
            current_id = task_id or self.current_task_id or 1
            if mux.owner_task_id != current_id:
                return LOS_NOK
            mux.lock_count -= 1
            if mux.lock_count == 0:
                # Restore owner base priority
                if current_id in self.tasks:
                    self.tasks[current_id].priority = self.tasks[current_id].base_priority
                if mux.wait_tasks:
                    next_owner = mux.wait_tasks.pop(0)
                    mux.owner_task_id = next_owner
                    mux.lock_count = 1
                else:
                    mux.owner_task_id = None
            return LOS_OK

    # ================= Semaphore =================
    def LOS_SemCreate(self, count: int, max_count: int = 65535) -> tuple[int, int]:
        with self._lock:
            if count > max_count or max_count == 0:
                return LOS_NOK, 0
            sem_id = self.next_sem_id
            self.next_sem_id += 1
            self.semaphores[sem_id] = LosSemCB(sem_id, count, max_count)
            return LOS_OK, sem_id

    def LOS_SemDelete(self, sem_id: int) -> int:
        with self._lock:
            if sem_id not in self.semaphores:
                return LOS_NOK
            del self.semaphores[sem_id]
            return LOS_OK

    def LOS_SemPend(self, sem_id: int, timeout: int = LOS_WAIT_FOREVER) -> int:
        with self._lock:
            if sem_id not in self.semaphores:
                return LOS_NOK
            sem = self.semaphores[sem_id]
            if sem.count > 0:
                sem.count -= 1
                return LOS_OK
            if timeout == LOS_NO_WAIT:
                return LOS_NOK
            return LOS_OK

    def LOS_SemPost(self, sem_id: int) -> int:
        with self._lock:
            if sem_id not in self.semaphores:
                return LOS_NOK
            sem = self.semaphores[sem_id]
            if sem.count >= sem.max_count:
                return LOS_NOK
            sem.count += 1
            return LOS_OK

    # ================= Message Queue =================
    def LOS_QueueCreate(self, queue_len: int, max_msg_size: int) -> tuple[int, int]:
        with self._lock:
            if queue_len <= 0 or max_msg_size <= 0:
                return LOS_NOK, 0
            qid = self.next_queue_id
            self.next_queue_id += 1
            self.queues[qid] = LosQueueCB(qid, queue_len, max_msg_size)
            return LOS_OK, qid

    def LOS_QueueDelete(self, queue_id: int) -> int:
        with self._lock:
            if queue_id not in self.queues:
                return LOS_NOK
            del self.queues[queue_id]
            return LOS_OK

    def LOS_QueueWrite(self, queue_id: int, buffer: Any, buffer_size: int, timeout: int = LOS_WAIT_FOREVER) -> int:
        with self._lock:
            if queue_id not in self.queues:
                return LOS_NOK
            q = self.queues[queue_id]
            if buffer_size > q.max_msg_size:
                return LOS_NOK
            if len(q.messages) >= q.queue_len:
                if timeout == LOS_NO_WAIT:
                    return LOS_NOK
                return LOS_NOK  # Queue full
            q.messages.append(buffer)
            return LOS_OK

    def LOS_QueueRead(self, queue_id: int, buffer_size: int, timeout: int = LOS_WAIT_FOREVER) -> tuple[int, Any]:
        with self._lock:
            if queue_id not in self.queues:
                return LOS_NOK, None
            q = self.queues[queue_id]
            if not q.messages:
                if timeout == LOS_NO_WAIT:
                    return LOS_NOK, None
                return LOS_NOK, None
            msg = q.messages.pop(0)
            return LOS_OK, msg

    def LOS_QueueInfoGet(self, queue_id: int) -> dict[str, int]:
        with self._lock:
            if queue_id not in self.queues:
                return {}
            q = self.queues[queue_id]
            return {
                "queue_len": q.queue_len,
                "msg_size": q.max_msg_size,
                "messages_in_queue": len(q.messages),
                "free_slots": q.queue_len - len(q.messages)
            }

    # ================= Software Timer =================
    def LOS_SwtmrCreate(self, interval: int, mode: int, handler: Callable, arg: Any = None) -> tuple[int, int]:
        with self._lock:
            if interval <= 0 or mode not in (LOS_SWTMR_MODE_ONCE, LOS_SWTMR_MODE_PERIOD, LOS_SWTMR_MODE_NO_SELFDELETE):
                return LOS_NOK, 0
            tid = self.next_timer_id
            self.next_timer_id += 1
            self.timers[tid] = LosSwtmrCB(tid, interval, mode, handler, arg)
            return LOS_OK, tid

    def LOS_SwtmrStart(self, timer_id: int) -> int:
        with self._lock:
            if timer_id not in self.timers:
                return LOS_NOK
            tmr = self.timers[timer_id]
            tmr.state = "RUNNING"
            tmr.ticks_remaining = tmr.interval
            return LOS_OK

    def LOS_SwtmrStop(self, timer_id: int) -> int:
        with self._lock:
            if timer_id not in self.timers:
                return LOS_NOK
            tmr = self.timers[timer_id]
            tmr.state = "STOPPED"
            return LOS_OK

    def LOS_SwtmrDelete(self, timer_id: int) -> int:
        with self._lock:
            if timer_id not in self.timers:
                return LOS_NOK
            del self.timers[timer_id]
            return LOS_OK

    def LOS_SwtmrTimeGet(self, timer_id: int) -> int:
        with self._lock:
            if timer_id not in self.timers:
                return 0
            return self.timers[timer_id].ticks_remaining

    def LOS_SwtmrTickProcess(self, ticks: int = 1) -> None:
        with self._lock:
            self.tick_count += ticks
            to_delete = []
            for tid, tmr in list(self.timers.items()):
                if tmr.state == "RUNNING":
                    tmr.ticks_remaining -= ticks
                    if tmr.ticks_remaining <= 0:
                        tmr.trigger_count += 1
                        if callable(tmr.handler):
                            try:
                                if tmr.arg is not None:
                                    tmr.handler(tmr.arg)
                                else:
                                    tmr.handler()
                            except Exception:
                                pass
                        if tmr.mode == LOS_SWTMR_MODE_ONCE:
                            tmr.state = "STOPPED"
                            to_delete.append(tid)
                        elif tmr.mode == LOS_SWTMR_MODE_NO_SELFDELETE:
                            tmr.state = "STOPPED"
                        else:  # PERIODIC
                            tmr.ticks_remaining = tmr.interval
            for tid in to_delete:
                if tid in self.timers:
                    del self.timers[tid]

    # ================= Memory Pool =================
    def LOS_MemAlloc(self, size: int) -> Optional[int]:
        return self.mempool.alloc(size)

    def LOS_MemFree(self, ptr: int) -> bool:
        return self.mempool.free(ptr)

    def LOS_MemRealloc(self, ptr: int, new_size: int) -> Optional[int]:
        return self.mempool.realloc(ptr, new_size)

    def LOS_MemInfoGet(self) -> dict[str, int]:
        return self.mempool.get_info()

"""
OpenHarmony POSIX API Compatibility Layer for ESP32 Architecture
Subsystem: POSIX Standard (pthread, mutex, sched_yield, clock_gettime, errno, syscalls)
"""

import threading
import time
from typing import Any, Callable, Optional

# POSIX Return Codes
EOK = 0
EINVAL = 22
EBUSY = 16
EDEADLK = 35
EPERM = 1
ESRCH = 3

# Clock Types
CLOCK_REALTIME = 0
CLOCK_MONOTONIC = 1

# Mutex Types
PTHREAD_MUTEX_NORMAL = 0
PTHREAD_MUTEX_RECURSIVE = 1
PTHREAD_MUTEX_ERRORCHECK = 2


class Timespec:
    """POSIX timespec structure: tv_sec and tv_nsec."""
    def __init__(self, tv_sec: int = 0, tv_nsec: int = 0):
        self.tv_sec = int(tv_sec)
        self.tv_nsec = int(tv_nsec)

    def to_float(self) -> float:
        return self.tv_sec + self.tv_nsec / 1e9

    def __repr__(self) -> str:
        return f"Timespec(tv_sec={self.tv_sec}, tv_nsec={self.tv_nsec})"


class PthreadAttr:
    """POSIX pthread attribute structure."""
    def __init__(self):
        self.stacksize: int = 4096
        self.sched_priority: int = 20
        self.detachstate: int = 0  # PTHREAD_CREATE_JOINABLE

    def setstacksize(self, size: int) -> int:
        if size < 1024 or size > 1024 * 1024:
            return EINVAL
        self.stacksize = size
        return EOK

    def getstacksize(self) -> int:
        return self.stacksize


class PthreadMutex:
    """POSIX pthread_mutex implementation."""
    def __init__(self, mutex_type: int = PTHREAD_MUTEX_NORMAL):
        self.mutex_type = mutex_type
        self.owner: Optional[int] = None
        self.lock_count: int = 0
        self._lock = threading.RLock()
        self.initialized = True

    def lock(self, thread_id: int) -> int:
        with self._lock:
            if not self.initialized:
                return EINVAL
            if self.owner is None:
                self.owner = thread_id
                self.lock_count = 1
                return EOK
            if self.owner == thread_id:
                if self.mutex_type == PTHREAD_MUTEX_RECURSIVE:
                    self.lock_count += 1
                    return EOK
                elif self.mutex_type == PTHREAD_MUTEX_ERRORCHECK:
                    return EDEADLK
                else:
                    return EDEADLK
            # Another thread holds lock
            return EBUSY

    def trylock(self, thread_id: int) -> int:
        with self._lock:
            if not self.initialized:
                return EINVAL
            if self.owner is None:
                self.owner = thread_id
                self.lock_count = 1
                return EOK
            if self.owner == thread_id and self.mutex_type == PTHREAD_MUTEX_RECURSIVE:
                self.lock_count += 1
                return EOK
            return EBUSY

    def unlock(self, thread_id: int) -> int:
        with self._lock:
            if not self.initialized:
                return EINVAL
            if self.owner != thread_id:
                return EPERM
            self.lock_count -= 1
            if self.lock_count == 0:
                self.owner = None
            return EOK

    def destroy(self) -> int:
        with self._lock:
            if self.owner is not None:
                return EBUSY
            self.initialized = False
            return EOK


class PosixThread:
    """Thread object wrapping execution and join value."""
    def __init__(self, tid: int, target: Callable, arg: Any = None, attr: Optional[PthreadAttr] = None):
        self.tid = tid
        self.target = target
        self.arg = arg
        self.attr = attr or PthreadAttr()
        self.result: Any = None
        self.is_finished = False
        self.is_detached = False
        self.errno: int = EOK

    def execute(self) -> None:
        try:
            if self.arg is not None:
                self.result = self.target(self.arg)
            else:
                self.result = self.target()
        except Exception:
            self.errno = EINVAL
        finally:
            self.is_finished = True


class PosixRuntime:
    """POSIX Subsystem Runtime conforming to OpenHarmony Mini System."""

    def __init__(self):
        self._lock = threading.RLock()
        self.threads: dict[int, PosixThread] = {}
        self.next_tid = 100
        self.yield_count = 0
        self._thread_local = threading.local()

    def get_errno(self) -> int:
        return getattr(self._thread_local, "errno", EOK)

    def set_errno(self, err: int) -> None:
        self._thread_local.errno = err

    def pthread_create(self, target: Callable, arg: Any = None, attr: Optional[PthreadAttr] = None) -> tuple[int, int]:
        with self._lock:
            tid = self.next_tid
            self.next_tid += 1
            pth = PosixThread(tid, target, arg, attr)
            self.threads[tid] = pth
            # In simulation, execute synchronously or schedule
            pth.execute()
            return EOK, tid

    def pthread_join(self, tid: int) -> tuple[int, Any]:
        with self._lock:
            if tid not in self.threads:
                return ESRCH, None
            pth = self.threads[tid]
            if pth.is_detached:
                return EINVAL, None
            if not pth.is_finished:
                pth.execute()
            res = pth.result
            del self.threads[tid]
            return EOK, res

    def pthread_detach(self, tid: int) -> int:
        with self._lock:
            if tid not in self.threads:
                return ESRCH
            pth = self.threads[tid]
            pth.is_detached = True
            return EOK

    def pthread_self(self) -> int:
        return 100  # Default current thread id

    def sched_yield(self) -> int:
        with self._lock:
            self.yield_count += 1
            return EOK

    def clock_gettime(self, clk_id: int) -> tuple[int, Timespec]:
        if clk_id == CLOCK_REALTIME:
            t = time.time()
            return EOK, Timespec(int(t), int((t - int(t)) * 1e9))
        elif clk_id == CLOCK_MONOTONIC:
            t = time.monotonic()
            return EOK, Timespec(int(t), int((t - int(t)) * 1e9))
        else:
            return EINVAL, Timespec(0, 0)

    def nanosleep(self, req: Timespec) -> int:
        if req.tv_nsec < 0 or req.tv_nsec >= 1e9:
            return EINVAL
        duration = req.to_float()
        time.sleep(min(duration, 0.005))  # Sleep up to 5ms in test harness
        return EOK

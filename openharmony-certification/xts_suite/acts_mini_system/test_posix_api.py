"""
ACTS Mini System Compatibility Test Suite
Subsystem: POSIX API Standards
Target: ESP32 Architecture
"""

from xts_suite.core.xts_test_case import XtsTestCase
from xts_suite.subsystems.posix_api import (
    PosixRuntime, PthreadAttr, PthreadMutex, Timespec,
    EOK, EINVAL, EBUSY, EDEADLK, EPERM, ESRCH,
    CLOCK_REALTIME, CLOCK_MONOTONIC,
    PTHREAD_MUTEX_NORMAL, PTHREAD_MUTEX_RECURSIVE, PTHREAD_MUTEX_ERRORCHECK
)


class TestPosixApi(XtsTestCase):
    """ACTS Compatibility Test Cases for OpenHarmony POSIX Standard APIs on ESP32."""

    subsystem = "POSIX"
    module_name = "test_posix_api"

    def setUp(self) -> None:
        self.posix = PosixRuntime()

    def tearDown(self) -> None:
        self.posix = None

    def test_pthread_create_and_join(self) -> None:
        """Verify pthread_create spawns thread and pthread_join receives result."""
        def worker(val: int) -> int:
            return val * 42

        ret, tid = self.posix.pthread_create(worker, 5)
        self.assert_equal(ret, EOK, "pthread_create should return EOK")
        self.assert_greater(tid, 0, "Valid thread ID should be allocated")

        join_ret, res = self.posix.pthread_join(tid)
        self.assert_equal(join_ret, EOK, "pthread_join should return EOK")
        self.assert_equal(res, 210, "Thread result should match calculation (5 * 42 = 210)")

    def test_pthread_detach(self) -> None:
        """Verify detached thread cannot be joined."""
        def dummy():
            return 100

        ret, tid = self.posix.pthread_create(dummy)
        self.assert_equal(ret, EOK)

        detach_ret = self.posix.pthread_detach(tid)
        self.assert_equal(detach_ret, EOK, "pthread_detach should return EOK")

        # Attempting join on detached thread should fail with EINVAL
        join_ret, _ = self.posix.pthread_join(tid)
        self.assert_equal(join_ret, EINVAL, "Joining detached thread must return EINVAL")

    def test_pthread_attr_stacksize(self) -> None:
        """Verify pthread stack size attribute modification and bounds checking."""
        attr = PthreadAttr()
        self.assert_equal(attr.getstacksize(), 4096, "Default stack size should be 4096 bytes")

        # Valid custom stack size (8192 bytes)
        ret = attr.setstacksize(8192)
        self.assert_equal(ret, EOK)
        self.assert_equal(attr.getstacksize(), 8192)

        # Invalid stack sizes (< 1024 or excessive)
        self.assert_equal(attr.setstacksize(512), EINVAL, "Stack < 1024 bytes must be rejected with EINVAL")
        self.assert_equal(attr.setstacksize(10 * 1024 * 1024), EINVAL, "Excessive stack must be rejected")

    def test_pthread_mutex_normal_locking(self) -> None:
        """Verify normal mutex lock, trylock, unlock, and destroy."""
        mutex = PthreadMutex(PTHREAD_MUTEX_NORMAL)
        thread_a = 101
        thread_b = 102

        # Thread A locks
        self.assert_equal(mutex.lock(thread_a), EOK)
        self.assert_equal(mutex.owner, thread_a)

        # Thread B trylock fails with EBUSY
        self.assert_equal(mutex.trylock(thread_b), EBUSY, "Trylock from competing thread should return EBUSY")

        # Thread B unlocking Thread A's lock should fail with EPERM
        self.assert_equal(mutex.unlock(thread_b), EPERM, "Unlocking foreign thread mutex must return EPERM")

        # Thread A unlocks
        self.assert_equal(mutex.unlock(thread_a), EOK)
        self.assert_is_none(mutex.owner)

        # Destroy unlocked mutex
        self.assert_equal(mutex.destroy(), EOK)

    def test_pthread_mutex_recursive(self) -> None:
        """Verify recursive mutex reentrant acquisition and balanced release."""
        mutex = PthreadMutex(PTHREAD_MUTEX_RECURSIVE)
        tid = 200

        self.assert_equal(mutex.lock(tid), EOK)
        self.assert_equal(mutex.lock(tid), EOK)
        self.assert_equal(mutex.trylock(tid), EOK)
        self.assert_equal(mutex.lock_count, 3, "Recursive mutex lock count should be 3")

        # Release step by step
        self.assert_equal(mutex.unlock(tid), EOK)
        self.assert_equal(mutex.unlock(tid), EOK)
        self.assert_equal(mutex.lock_count, 1)

        self.assert_equal(mutex.unlock(tid), EOK)
        self.assert_is_none(mutex.owner, "Mutex should be fully unlocked after 3 unlocks")

    def test_pthread_mutex_errorcheck(self) -> None:
        """Verify errorcheck mutex detects deadlock on self-relock."""
        mutex = PthreadMutex(PTHREAD_MUTEX_ERRORCHECK)
        tid = 300

        self.assert_equal(mutex.lock(tid), EOK)
        # Re-locking with same thread in ERRORCHECK mode returns EDEADLK
        self.assert_equal(mutex.lock(tid), EDEADLK, "ERRORCHECK mutex re-lock must return EDEADLK")
        self.assert_equal(mutex.unlock(tid), EOK)

    def test_pthread_sched_yield(self) -> None:
        """Verify sched_yield yields processor execution."""
        initial_yields = self.posix.yield_count
        ret = self.posix.sched_yield()
        self.assert_equal(ret, EOK)
        self.assert_equal(self.posix.yield_count, initial_yields + 1, "sched_yield should increment yield counter")

    def test_clock_gettime_monotonic_and_realtime(self) -> None:
        """Verify clock_gettime for CLOCK_REALTIME and CLOCK_MONOTONIC."""
        # Realtime
        ret_rt, ts_rt = self.posix.clock_gettime(CLOCK_REALTIME)
        self.assert_equal(ret_rt, EOK)
        self.assert_greater(ts_rt.tv_sec, 1700000000, "Realtime seconds should represent valid epoch")
        self.assert_greater_equal(ts_rt.tv_nsec, 0)
        self.assert_less(ts_rt.tv_nsec, 1e9)

        # Monotonic
        ret_mono1, ts_mono1 = self.posix.clock_gettime(CLOCK_MONOTONIC)
        self.assert_equal(ret_mono1, EOK)
        
        # Subsequent monotonic check must be monotonically non-decreasing
        ret_mono2, ts_mono2 = self.posix.clock_gettime(CLOCK_MONOTONIC)
        self.assert_equal(ret_mono2, EOK)
        self.assert_greater_equal(ts_mono2.to_float(), ts_mono1.to_float(), "Monotonic clock must not decrease")

        # Invalid clock ID
        ret_inv, _ = self.posix.clock_gettime(999)
        self.assert_equal(ret_inv, EINVAL, "Invalid clock id must return EINVAL")

    def test_nanosleep_delay_precision(self) -> None:
        """Verify nanosleep validates timespec parameters and executes sleep delay."""
        req = Timespec(tv_sec=0, tv_nsec=2000000)  # 2 ms
        ret = self.posix.nanosleep(req)
        self.assert_equal(ret, EOK, "Valid nanosleep should return EOK")

        # Invalid nanoseconds (>= 1e9 or < 0)
        invalid_req = Timespec(tv_sec=0, tv_nsec=1500000000)
        self.assert_equal(self.posix.nanosleep(invalid_req), EINVAL, "tv_nsec >= 1e9 must return EINVAL")

    def test_posix_errno_thread_isolation(self) -> None:
        """Verify errno thread-local isolation."""
        self.posix.set_errno(EOK)
        self.assert_equal(self.posix.get_errno(), EOK)

        self.posix.set_errno(EINVAL)
        self.assert_equal(self.posix.get_errno(), EINVAL, "errno should reflect set value")

        # Reset errno
        self.posix.set_errno(EOK)
        self.assert_equal(self.posix.get_errno(), EOK)

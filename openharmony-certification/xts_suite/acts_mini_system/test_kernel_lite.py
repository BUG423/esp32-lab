"""
ACTS Mini System Compatibility Test Suite
Subsystem: Kernel Lite (LiteOS-M)
Target: ESP32 Architecture
"""

from xts_suite.core.xts_test_case import XtsTestCase
from xts_suite.subsystems.kernel_lite import (
    LiteOSMKernel, LOS_OK, LOS_NOK, LOS_NO_WAIT, LOS_WAIT_FOREVER,
    LOS_TASK_PRIORITY_HIGHEST, LOS_TASK_PRIORITY_LOWEST,
    LOS_SWTMR_MODE_ONCE, LOS_SWTMR_MODE_PERIOD, LOS_SWTMR_MODE_NO_SELFDELETE,
    TASK_STATUS_READY, TASK_STATUS_SUSPENDED
)


class TestKernelLite(XtsTestCase):
    """ACTS Compatibility Test Cases for OpenHarmony LiteOS-M Kernel on ESP32."""
    
    subsystem = "Kernel"
    module_name = "test_kernel_lite"

    def setUp(self) -> None:
        self.kernel = LiteOSMKernel()

    def tearDown(self) -> None:
        self.kernel = None

    def test_task_create_delete(self) -> None:
        """Verify task creation with valid attributes and subsequent clean deletion."""
        executed = []
        def task_func():
            executed.append("done")

        ret, tid = self.kernel.LOS_TaskCreate("task_worker", task_func, priority=10, stack_size=2048)
        self.assert_equal(ret, LOS_OK, "Task creation should return LOS_OK")
        self.assert_greater(tid, 0, "Valid Task ID must be returned")
        self.assert_in(tid, self.kernel.tasks, "Task should exist in kernel task map")
        self.assert_equal(self.kernel.tasks[tid].priority, 10, "Task priority should match requested priority")
        self.assert_equal(self.kernel.tasks[tid].status, TASK_STATUS_READY, "Initial task status should be READY")

        # Execute task
        scheduled_tid = self.kernel.schedule_next()
        self.assert_equal(scheduled_tid, tid, "Scheduled task must match created task ID")
        self.assert_equal(executed, ["done"], "Task entry function must be executed")

        # Delete task
        del_ret = self.kernel.LOS_TaskDelete(tid)
        self.assert_equal(del_ret, LOS_OK, "Task deletion should return LOS_OK")
        self.assert_not_in(tid, self.kernel.tasks, "Deleted task should no longer exist in kernel task map")

    def test_task_preemption_priority(self) -> None:
        """Verify that high-priority tasks immediately preempt lower-priority tasks."""
        order = []
        def low_pri():
            order.append("low")

        def high_pri():
            order.append("high")

        # Create low priority task (pri=25) and high priority task (pri=5)
        ret_low, tid_low = self.kernel.LOS_TaskCreate("low_task", low_pri, priority=25)
        ret_high, tid_high = self.kernel.LOS_TaskCreate("high_task", high_pri, priority=5)
        self.assert_equal(ret_low, LOS_OK)
        self.assert_equal(ret_high, LOS_OK)

        # First scheduled task must be the higher priority task
        first_scheduled = self.kernel.schedule_next()
        self.assert_equal(first_scheduled, tid_high, "High priority task must be scheduled first")
        
        # Second scheduled task is lower priority
        second_scheduled = self.kernel.schedule_next()
        self.assert_equal(second_scheduled, tid_low, "Low priority task scheduled after high priority")
        self.assert_equal(order, ["high", "low"], "Execution order must strictly follow priority preemption")

    def test_task_round_robin_scheduling(self) -> None:
        """Verify cooperative and round-robin scheduling for equal priority tasks."""
        trace = []
        def t1():
            trace.append("t1")
        def t2():
            trace.append("t2")

        ret1, tid1 = self.kernel.LOS_TaskCreate("task1", t1, priority=15)
        ret2, tid2 = self.kernel.LOS_TaskCreate("task2", t2, priority=15)
        self.assert_equal(ret1, LOS_OK)
        self.assert_equal(ret2, LOS_OK)

        s1 = self.kernel.schedule_next()
        s2 = self.kernel.schedule_next()
        self.assert_not_equal(s1, None)
        self.assert_not_equal(s2, None)
        self.assert_equal(trace, ["t1", "t2"], "Equal priority tasks should both execute in ready order")

    def test_task_suspend_resume(self) -> None:
        """Verify task suspend removes task from ready queue, and resume restores readiness."""
        def dummy():
            pass

        ret, tid = self.kernel.LOS_TaskCreate("suspendable", dummy, priority=12)
        self.assert_equal(ret, LOS_OK)
        self.assert_in(tid, self.kernel.ready_queue, "Task should initially be in ready queue")

        # Suspend
        sus_ret = self.kernel.LOS_TaskSuspend(tid)
        self.assert_equal(sus_ret, LOS_OK, "LOS_TaskSuspend must return LOS_OK")
        self.assert_equal(self.kernel.tasks[tid].status, TASK_STATUS_SUSPENDED)
        self.assert_not_in(tid, self.kernel.ready_queue, "Suspended task must not be in ready queue")

        # Double suspend should fail
        self.assert_equal(self.kernel.LOS_TaskSuspend(tid), LOS_NOK, "Suspending already suspended task should fail")

        # Resume
        res_ret = self.kernel.LOS_TaskResume(tid)
        self.assert_equal(res_ret, LOS_OK, "LOS_TaskResume must return LOS_OK")
        self.assert_equal(self.kernel.tasks[tid].status, TASK_STATUS_READY)
        self.assert_in(tid, self.kernel.ready_queue, "Resumed task must be back in ready queue")

    def test_task_pri_set_get(self) -> None:
        """Verify dynamic priority modification and query."""
        def dummy():
            pass

        ret, tid = self.kernel.LOS_TaskCreate("pri_test", dummy, priority=20)
        self.assert_equal(ret, LOS_OK)
        self.assert_equal(self.kernel.LOS_TaskPriGet(tid), 20)

        # Update priority to 8
        self.assert_equal(self.kernel.LOS_TaskPriSet(tid, 8), LOS_OK)
        self.assert_equal(self.kernel.LOS_TaskPriGet(tid), 8, "Priority should be updated to 8")

        # Out-of-bounds priority should fail
        self.assert_equal(self.kernel.LOS_TaskPriSet(tid, 99), LOS_NOK, "Priority > 31 must be rejected")
        self.assert_equal(self.kernel.LOS_TaskPriSet(tid, -5), LOS_NOK, "Priority < 0 must be rejected")

    def test_mutex_basic_lock_unlock(self) -> None:
        """Verify mutex creation, locking, unlocking and deletion."""
        ret, mux_id = self.kernel.LOS_MuxCreate()
        self.assert_equal(ret, LOS_OK, "LOS_MuxCreate should return LOS_OK")
        self.assert_greater(mux_id, 0)

        # Lock
        pend_ret = self.kernel.LOS_MuxPend(mux_id, LOS_WAIT_FOREVER, task_id=101)
        self.assert_equal(pend_ret, LOS_OK, "LOS_MuxPend should succeed")
        self.assert_equal(self.kernel.mutexes[mux_id].owner_task_id, 101)

        # Unlock
        post_ret = self.kernel.LOS_MuxPost(mux_id, task_id=101)
        self.assert_equal(post_ret, LOS_OK, "LOS_MuxPost should succeed")
        self.assert_is_none(self.kernel.mutexes[mux_id].owner_task_id, "Mutex owner should be None after release")

        # Delete
        del_ret = self.kernel.LOS_MuxDelete(mux_id)
        self.assert_equal(del_ret, LOS_OK, "LOS_MuxDelete should succeed on unlocked mutex")

    def test_mutex_recursive_lock(self) -> None:
        """Verify recursive reentrant locking by the same owner task."""
        ret, mux_id = self.kernel.LOS_MuxCreate()
        self.assert_equal(ret, LOS_OK)

        # Lock three times by same task
        self.assert_equal(self.kernel.LOS_MuxPend(mux_id, task_id=200), LOS_OK)
        self.assert_equal(self.kernel.LOS_MuxPend(mux_id, task_id=200), LOS_OK)
        self.assert_equal(self.kernel.LOS_MuxPend(mux_id, task_id=200), LOS_OK)
        self.assert_equal(self.kernel.mutexes[mux_id].lock_count, 3, "Recursive lock count should be 3")

        # Release twice - still held
        self.assert_equal(self.kernel.LOS_MuxPost(mux_id, task_id=200), LOS_OK)
        self.assert_equal(self.kernel.LOS_MuxPost(mux_id, task_id=200), LOS_OK)
        self.assert_equal(self.kernel.mutexes[mux_id].lock_count, 1)
        self.assert_equal(self.kernel.mutexes[mux_id].owner_task_id, 200)

        # Third release - fully unlocked
        self.assert_equal(self.kernel.LOS_MuxPost(mux_id, task_id=200), LOS_OK)
        self.assert_is_none(self.kernel.mutexes[mux_id].owner_task_id)

    def test_mutex_priority_inheritance(self) -> None:
        """Verify Priority Inheritance Protocol elevates lower priority task holding mutex."""
        def t_low():
            pass
        def t_high():
            pass

        _, tid_low = self.kernel.LOS_TaskCreate("t_low", t_low, priority=25)
        _, tid_high = self.kernel.LOS_TaskCreate("t_high", t_high, priority=5)
        _, mux_id = self.kernel.LOS_MuxCreate()

        # Low priority task acquires mutex
        self.kernel.LOS_MuxPend(mux_id, task_id=tid_low)
        self.assert_equal(self.kernel.tasks[tid_low].priority, 25)

        # High priority task attempts to pend mutex
        self.kernel.LOS_MuxPend(mux_id, task_id=tid_high)
        # Priority inheritance: tid_low priority elevated to tid_high (pri=5)
        self.assert_equal(self.kernel.tasks[tid_low].priority, 5, "Low priority task must inherit high priority")

        # Low priority releases mutex -> priority restored to base priority (25)
        self.kernel.LOS_MuxPost(mux_id, task_id=tid_low)
        self.assert_equal(self.kernel.tasks[tid_low].priority, 25, "Priority must be restored upon release")
        self.assert_equal(self.kernel.mutexes[mux_id].owner_task_id, tid_high, "Mutex should now belong to high priority task")

    def test_mutex_cannot_delete_locked(self) -> None:
        """Verify that attempting to delete a currently locked mutex returns LOS_NOK."""
        _, mux_id = self.kernel.LOS_MuxCreate()
        self.kernel.LOS_MuxPend(mux_id, task_id=1)
        self.assert_equal(self.kernel.LOS_MuxDelete(mux_id), LOS_NOK, "Cannot delete locked mutex")
        self.kernel.LOS_MuxPost(mux_id, task_id=1)
        self.assert_equal(self.kernel.LOS_MuxDelete(mux_id), LOS_OK, "Can delete once unlocked")

    def test_semaphore_counting_and_binary(self) -> None:
        """Verify semaphore initialization, counting limits and non-blocking pend."""
        # Binary semaphore (init=1, max=1)
        ret, sem_bin = self.kernel.LOS_SemCreate(count=1, max_count=1)
        self.assert_equal(ret, LOS_OK)
        self.assert_equal(self.kernel.LOS_SemPend(sem_bin, LOS_NO_WAIT), LOS_OK)
        self.assert_equal(self.kernel.LOS_SemPend(sem_bin, LOS_NO_WAIT), LOS_NOK, "Second pend on empty sem should fail")
        self.assert_equal(self.kernel.LOS_SemPost(sem_bin), LOS_OK)
        self.assert_equal(self.kernel.LOS_SemPost(sem_bin), LOS_NOK, "Post beyond max_count must fail")

        # Counting semaphore (init=3, max=5)
        ret, sem_cnt = self.kernel.LOS_SemCreate(count=3, max_count=5)
        self.assert_equal(ret, LOS_OK)
        self.assert_equal(self.kernel.semaphores[sem_cnt].count, 3)
        self.kernel.LOS_SemPost(sem_cnt)
        self.assert_equal(self.kernel.semaphores[sem_cnt].count, 4)

    def test_semaphore_sync_producer_consumer(self) -> None:
        """Verify producer-consumer synchronization via counting semaphore."""
        _, sem_items = self.kernel.LOS_SemCreate(count=0, max_count=10)
        consumed = []

        # Producer posts 3 items
        for i in range(3):
            self.kernel.LOS_SemPost(sem_items)

        # Consumer pends 3 items
        for i in range(3):
            ret = self.kernel.LOS_SemPend(sem_items, LOS_WAIT_FOREVER)
            if ret == LOS_OK:
                consumed.append(i)

        self.assert_equal(len(consumed), 3, "Consumer should have successfully pended 3 items")
        self.assert_equal(self.kernel.semaphores[sem_items].count, 0, "Semaphore count should be exhausted to 0")

    def test_queue_fifo_read_write(self) -> None:
        """Verify message queue FIFO ordering and message integrity."""
        ret, qid = self.kernel.LOS_QueueCreate(queue_len=4, max_msg_size=32)
        self.assert_equal(ret, LOS_OK)
        self.assert_greater(qid, 0)

        # Write messages in order
        messages = [b"MSG_ALPHA", b"MSG_BETA", b"MSG_GAMMA"]
        for msg in messages:
            w_ret = self.kernel.LOS_QueueWrite(qid, msg, len(msg))
            self.assert_equal(w_ret, LOS_OK, f"Writing {msg} should succeed")

        info = self.kernel.LOS_QueueInfoGet(qid)
        self.assert_equal(info["messages_in_queue"], 3)
        self.assert_equal(info["free_slots"], 1)

        # Read back in FIFO order
        for expected in messages:
            r_ret, read_msg = self.kernel.LOS_QueueRead(qid, 32)
            self.assert_equal(r_ret, LOS_OK)
            self.assert_equal(read_msg, expected, "Message must preserve strict FIFO order")

        # Queue should now be empty
        empty_ret, _ = self.kernel.LOS_QueueRead(qid, 32, timeout=LOS_NO_WAIT)
        self.assert_equal(empty_ret, LOS_NOK, "Read from empty queue should return LOS_NOK")

    def test_queue_overflow_and_timeout(self) -> None:
        """Verify queue overflow protection when writing beyond queue length."""
        _, qid = self.kernel.LOS_QueueCreate(queue_len=2, max_msg_size=16)
        self.kernel.LOS_QueueWrite(qid, b"M1", 2)
        self.kernel.LOS_QueueWrite(qid, b"M2", 2)
        # 3rd write exceeds capacity
        overflow_ret = self.kernel.LOS_QueueWrite(qid, b"M3", 2, timeout=LOS_NO_WAIT)
        self.assert_equal(overflow_ret, LOS_NOK, "Queue overflow write should be rejected")

    def test_swtmr_single_shot_and_periodic(self) -> None:
        """Verify software timer single shot and periodic triggering behaviors."""
        single_triggers = []
        periodic_triggers = []

        def single_cb():
            single_triggers.append(1)

        def periodic_cb():
            periodic_triggers.append(1)

        # Single-shot timer (interval = 3 ticks)
        _, tmr_single = self.kernel.LOS_SwtmrCreate(3, LOS_SWTMR_MODE_ONCE, single_cb)
        self.kernel.LOS_SwtmrStart(tmr_single)

        # Periodic timer (interval = 2 ticks)
        _, tmr_period = self.kernel.LOS_SwtmrCreate(2, LOS_SWTMR_MODE_PERIOD, periodic_cb)
        self.kernel.LOS_SwtmrStart(tmr_period)

        # Advance 2 ticks: periodic triggers once, single-shot not yet
        self.kernel.LOS_SwtmrTickProcess(2)
        self.assert_equal(len(periodic_triggers), 1, "Periodic timer should trigger at tick 2")
        self.assert_equal(len(single_triggers), 0, "Single-shot timer should not trigger yet")

        # Advance 2 more ticks (total 4): periodic triggers again (total 2), single-shot triggers (total 1)
        self.kernel.LOS_SwtmrTickProcess(2)
        self.assert_equal(len(single_triggers), 1, "Single-shot timer should trigger")
        self.assert_equal(len(periodic_triggers), 2, "Periodic timer should trigger twice")

        # Advance 2 more ticks: single-shot should NOT trigger again (auto-deleted)
        self.kernel.LOS_SwtmrTickProcess(2)
        self.assert_equal(len(single_triggers), 1, "Single-shot timer must not re-trigger")
        self.assert_equal(len(periodic_triggers), 3, "Periodic timer should trigger a 3rd time")

    def test_mempool_alloc_realloc_free(self) -> None:
        """Verify dynamic memory pool allocation, reallocation, and boundary free."""
        # Initial info
        info = self.kernel.LOS_MemInfoGet()
        self.assert_equal(info["used_size"], 0)
        self.assert_equal(info["active_allocations"], 0)

        # Allocate 100 bytes
        ptr1 = self.kernel.LOS_MemAlloc(100)
        self.assert_is_not_none(ptr1, "Allocation of 100 bytes should return valid pointer")
        info = self.kernel.LOS_MemInfoGet()
        self.assert_greater(info["used_size"], 0)
        self.assert_equal(info["active_allocations"], 1)

        # Allocate another 256 bytes
        ptr2 = self.kernel.LOS_MemAlloc(256)
        self.assert_is_not_none(ptr2)
        self.assert_not_equal(ptr1, ptr2, "Different allocations must have distinct addresses")

        # Realloc ptr1 to 200 bytes
        new_ptr1 = self.kernel.LOS_MemRealloc(ptr1, 200)
        self.assert_is_not_none(new_ptr1, "Reallocation should succeed")

        # Free blocks
        self.assert_true(self.kernel.LOS_MemFree(new_ptr1), "Freeing valid pointer should succeed")
        self.assert_true(self.kernel.LOS_MemFree(ptr2), "Freeing second pointer should succeed")
        self.assert_false(self.kernel.LOS_MemFree(0x9999), "Freeing non-existent pointer should return False")

        # Memory should return to 0 used
        final_info = self.kernel.LOS_MemInfoGet()
        self.assert_equal(final_info["used_size"], 0, "All allocated memory must be reclaimed")
        self.assert_equal(final_info["active_allocations"], 0)

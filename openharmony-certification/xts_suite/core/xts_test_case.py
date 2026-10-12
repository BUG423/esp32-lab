"""
OpenHarmony XTS (X Test Suite) Base Test Case Framework
Target: OpenHarmony Mini System on ESP32 Architecture
"""

import time
import inspect
from typing import Any, Callable, Type


class XtsAssertionError(AssertionError):
    """Specific assertion exception for XTS Test Suite."""
    pass


class XtsTestCase:
    """Base class for OpenHarmony Mini System XTS test cases."""
    
    subsystem: str = "core"
    module_name: str = "generic"

    def __init__(self, methodName: str = "runTest"):
        self._test_method_name = methodName
        self.assertions_count: int = 0
        self.assertions_passed: int = 0
        self.assertions_failed: int = 0
        self.execution_time_ms: float = 0.0
        self.memory_used_bytes: int = 0
        self.status: str = "NOT_RUN"
        self.error_message: str = ""
        self.assertion_records: list[dict[str, Any]] = []

    def setUp(self) -> None:
        """Executed before each test method."""
        pass

    def tearDown(self) -> None:
        """Executed after each test method."""
        pass

    def _record_assertion(self, passed: bool, message: str, detail: str = "") -> None:
        self.assertions_count += 1
        if passed:
            self.assertions_passed += 1
        else:
            self.assertions_failed += 1
        self.assertion_records.append({
            "assert_index": self.assertions_count,
            "passed": passed,
            "message": message,
            "detail": detail,
            "time_offset_ms": round(time.perf_counter() * 1000, 3)
        })

    def assert_true(self, condition: Any, msg: str = "Condition expected to be True") -> None:
        if bool(condition):
            self._record_assertion(True, msg)
        else:
            self._record_assertion(False, msg, f"Actual condition was {condition}")
            raise XtsAssertionError(f"{msg} (Actual: {condition})")

    def assert_false(self, condition: Any, msg: str = "Condition expected to be False") -> None:
        if not bool(condition):
            self._record_assertion(True, msg)
        else:
            self._record_assertion(False, msg, f"Actual condition was {condition}")
            raise XtsAssertionError(f"{msg} (Actual: {condition})")

    def assert_equal(self, first: Any, second: Any, msg: str = "") -> None:
        if first == second:
            self._record_assertion(True, msg or f"{first} == {second}")
        else:
            fail_msg = msg or f"Expected {second!r}, got {first!r}"
            self._record_assertion(False, fail_msg, f"First: {first!r}, Second: {second!r}")
            raise XtsAssertionError(fail_msg)

    def assert_not_equal(self, first: Any, second: Any, msg: str = "") -> None:
        if first != second:
            self._record_assertion(True, msg or f"{first} != {second}")
        else:
            fail_msg = msg or f"Values are unexpectedly equal: {first!r}"
            self._record_assertion(False, fail_msg, f"Both values: {first!r}")
            raise XtsAssertionError(fail_msg)

    def assert_is_none(self, obj: Any, msg: str = "Expected None") -> None:
        if obj is None:
            self._record_assertion(True, msg)
        else:
            fail_msg = f"{msg} (Actual: {obj!r})"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)

    def assert_is_not_none(self, obj: Any, msg: str = "Expected not None") -> None:
        if obj is not None:
            self._record_assertion(True, msg)
        else:
            fail_msg = f"{msg} (Actual is None)"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)

    def assert_in(self, member: Any, container: Any, msg: str = "") -> None:
        if member in container:
            self._record_assertion(True, msg or f"{member!r} in container")
        else:
            fail_msg = msg or f"{member!r} not found in {container!r}"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)

    def assert_not_in(self, member: Any, container: Any, msg: str = "") -> None:
        if member not in container:
            self._record_assertion(True, msg or f"{member!r} not in container")
        else:
            fail_msg = msg or f"{member!r} unexpectedly found in {container!r}"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)

    def assert_greater(self, a: Any, b: Any, msg: str = "") -> None:
        if a > b:
            self._record_assertion(True, msg or f"{a} > {b}")
        else:
            fail_msg = msg or f"{a} is not greater than {b}"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)

    def assert_greater_equal(self, a: Any, b: Any, msg: str = "") -> None:
        if a >= b:
            self._record_assertion(True, msg or f"{a} >= {b}")
        else:
            fail_msg = msg or f"{a} is not greater than or equal to {b}"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)

    def assert_less(self, a: Any, b: Any, msg: str = "") -> None:
        if a < b:
            self._record_assertion(True, msg or f"{a} < {b}")
        else:
            fail_msg = msg or f"{a} is not less than {b}"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)

    def assert_less_equal(self, a: Any, b: Any, msg: str = "") -> None:
        if a <= b:
            self._record_assertion(True, msg or f"{a} <= {b}")
        else:
            fail_msg = msg or f"{a} is not less than or equal to {b}"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)

    def assert_raises(self, expected_exception: Type[BaseException], callable_obj: Callable, *args: Any, **kwargs: Any) -> None:
        try:
            callable_obj(*args, **kwargs)
        except expected_exception:
            self._record_assertion(True, f"Successfully raised expected {expected_exception.__name__}")
            return
        except Exception as e:
            fail_msg = f"Expected exception {expected_exception.__name__}, but caught {type(e).__name__}: {e}"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)
        else:
            fail_msg = f"Expected exception {expected_exception.__name__} was not raised"
            self._record_assertion(False, fail_msg)
            raise XtsAssertionError(fail_msg)

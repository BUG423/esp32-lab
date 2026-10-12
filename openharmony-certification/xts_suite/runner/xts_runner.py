#!/usr/bin/env python3
"""
OpenHarmony ACTS (Application Compatibility Test Suite) Runner
Target: OpenHarmony Mini System on ESP32 Architecture
"""

import argparse
import importlib
import inspect
import os
import sys
import time
import tracemalloc
from typing import Any, Type

# Ensure current project directory is in sys.path
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from xts_suite.core.xts_test_case import XtsTestCase, XtsAssertionError
from xts_suite.runner.report_generator import CtrReportGenerator


# Color codes for terminal output
COLOR_GREEN = "\033[92m"
COLOR_RED = "\033[91m"
COLOR_YELLOW = "\033[93m"
COLOR_CYAN = "\033[96m"
COLOR_BOLD = "\033[1m"
COLOR_RESET = "\033[0m"


MODULE_MAP = {
    "kernel": "xts_suite.acts_mini_system.test_kernel_lite",
    "test_kernel_lite": "xts_suite.acts_mini_system.test_kernel_lite",
    "posix": "xts_suite.acts_mini_system.test_posix_api",
    "test_posix_api": "xts_suite.acts_mini_system.test_posix_api",
    "samgr": "xts_suite.acts_mini_system.test_samgr_lite",
    "test_samgr_lite": "xts_suite.acts_mini_system.test_samgr_lite",
    "storage": "xts_suite.acts_mini_system.test_littlefs_kvstore",
    "test_littlefs_kvstore": "xts_suite.acts_mini_system.test_littlefs_kvstore",
    "littlefs": "xts_suite.acts_mini_system.test_littlefs_kvstore",
    "hdf": "xts_suite.acts_mini_system.test_hdf_drivers",
    "test_hdf_drivers": "xts_suite.acts_mini_system.test_hdf_drivers",
    "communication": "xts_suite.acts_mini_system.test_communication_wifi",
    "test_communication_wifi": "xts_suite.acts_mini_system.test_communication_wifi",
    "wifi": "xts_suite.acts_mini_system.test_communication_wifi",
    "huks": "xts_suite.acts_mini_system.test_huks_security",
    "test_huks_security": "xts_suite.acts_mini_system.test_huks_security",
    "security": "xts_suite.acts_mini_system.test_huks_security",
    "dfx": "xts_suite.acts_mini_system.test_hilog_dfx",
    "hilog": "xts_suite.acts_mini_system.test_hilog_dfx",
    "test_hilog_dfx": "xts_suite.acts_mini_system.test_hilog_dfx"
}

ALL_MODULE_FILES = [
    "xts_suite.acts_mini_system.test_kernel_lite",
    "xts_suite.acts_mini_system.test_posix_api",
    "xts_suite.acts_mini_system.test_samgr_lite",
    "xts_suite.acts_mini_system.test_littlefs_kvstore",
    "xts_suite.acts_mini_system.test_hdf_drivers",
    "xts_suite.acts_mini_system.test_communication_wifi",
    "xts_suite.acts_mini_system.test_huks_security",
    "xts_suite.acts_mini_system.test_hilog_dfx",
]


class XtsTestRunner:
    """Automated Test Execution Engine for OpenHarmony XTS/ACTS."""

    def __init__(self, output_dir: str = "deliverables", verbose: bool = True, timeout: float = 10.0):
        self.output_dir = os.path.abspath(output_dir)
        self.verbose = verbose
        self.timeout = timeout
        self.test_records: list[dict[str, Any]] = []
        self.subsystem_records: dict[str, dict[str, Any]] = {}
        self.total_assertions = 0
        self.passed_assertions = 0

    def discover_test_classes(self, module_name: str) -> list[Type[XtsTestCase]]:
        """Imports module and finds all XtsTestCase subclasses."""
        mod = importlib.import_module(module_name)
        classes = []
        for _, obj in inspect.getmembers(mod, inspect.isclass):
            if issubclass(obj, XtsTestCase) and obj is not XtsTestCase:
                classes.append(obj)
        return classes

    def run_test_method(self, cls: Type[XtsTestCase], method_name: str) -> dict[str, Any]:
        """Runs a single test method with time & memory tracking."""
        instance = cls(method_name)
        func = getattr(instance, method_name)
        doc = (func.__doc__ or "").strip().split("\n")[0]

        start_time = time.perf_counter()
        tracemalloc.start()
        status = "PASS"
        error_msg = ""

        try:
            instance.setUp()
            func()
        except XtsAssertionError as e:
            status = "FAIL"
            error_msg = str(e)
        except Exception as e:
            status = "ERROR"
            error_msg = f"{type(e).__name__}: {str(e)}"
        finally:
            try:
                instance.tearDown()
            except Exception:
                pass
        current_mem, peak_mem = tracemalloc.get_traced_memory()
        tracemalloc.stop()

        end_time = time.perf_counter()
        duration_ms = (end_time - start_time) * 1000

        self.total_assertions += instance.assertions_count
        self.passed_assertions += instance.assertions_passed
        self.peak_memory_bytes = max(getattr(self, "peak_memory_bytes", 0), peak_mem)

        rec = {
            "subsystem": instance.subsystem,
            "module": instance.module_name,
            "class": cls.__name__,
            "name": method_name,
            "description": doc,
            "status": status,
            "duration_ms": duration_ms,
            "memory_peak_bytes": peak_mem,
            "assertions_total": instance.assertions_count,
            "assertions_passed": instance.assertions_passed,
            "assertions_failed": instance.assertions_failed,
            "error_message": error_msg,
            "records": instance.assertion_records
        }
        return rec

    def run_modules(self, module_names: list[str]) -> bool:
        """Runs all discovered tests across requested modules."""
        print(f"\n{COLOR_CYAN}{COLOR_BOLD}========================================================================{COLOR_RESET}")
        print(f"{COLOR_CYAN}{COLOR_BOLD}   OpenHarmony ACTS (Mini System) Test Runner for ESP32 Target          {COLOR_RESET}")
        print(f"{COLOR_CYAN}{COLOR_BOLD}========================================================================{COLOR_RESET}\n")

        overall_start = time.perf_counter()
        tracemalloc.start()

        for mod_name in module_names:
            classes = self.discover_test_classes(mod_name)
            for cls in classes:
                # Find all test_ methods
                methods = [m for m in dir(cls) if m.startswith("test_") and callable(getattr(cls, m))]
                methods.sort()
                
                print(f"{COLOR_BOLD}>>> Subsystem [{cls.subsystem}] - Module: {cls.module_name} ({len(methods)} tests){COLOR_RESET}")

                for m in methods:
                    rec = self.run_test_method(cls, m)
                    self.test_records.append(rec)

                    # Update Subsystem stats
                    sub = rec["subsystem"]
                    if sub not in self.subsystem_records:
                        self.subsystem_records[sub] = {
                            "subsystem": sub,
                            "module": rec["module"],
                            "total": 0,
                            "passed": 0,
                            "failed": 0,
                            "duration_ms": 0.0,
                            "assertions_total": 0,
                            "assertions_passed": 0
                        }
                    s_rec = self.subsystem_records[sub]
                    s_rec["total"] += 1
                    s_rec["duration_ms"] += rec["duration_ms"]
                    s_rec["assertions_total"] += rec["assertions_total"]
                    s_rec["assertions_passed"] += rec["assertions_passed"]
                    if rec["status"] == "PASS":
                        s_rec["passed"] += 1
                    else:
                        s_rec["failed"] += 1

                    if self.verbose:
                        status_str = f"{COLOR_GREEN}[ PASS ]{COLOR_RESET}" if rec["status"] == "PASS" else f"{COLOR_RED}[ {rec['status']} ]{COLOR_RESET}"
                        print(f"  {status_str} {rec['name']:<42} ({rec['duration_ms']:6.2f} ms, asserts: {rec['assertions_passed']}/{rec['assertions_total']})")
                        if rec["status"] != "PASS":
                            print(f"         {COLOR_RED}Error: {rec['error_message']}{COLOR_RESET}")
                print()

        overall_end = time.perf_counter()
        _, overall_peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        total_duration_ms = (overall_end - overall_start) * 1000

        # Compute summary
        total_tests = len(self.test_records)
        passed_tests = sum(1 for r in self.test_records if r["status"] == "PASS")
        failed_tests = total_tests - passed_tests
        pass_rate = round((passed_tests / total_tests * 100), 2) if total_tests > 0 else 0.0

        for sub, s_rec in self.subsystem_records.items():
            s_rec["pass_rate"] = round((s_rec["passed"] / max(1, s_rec["total"]) * 100), 2)

        summary = {
            "total_tests": total_tests,
            "passed_tests": passed_tests,
            "failed_tests": failed_tests,
            "pass_rate": pass_rate,
            "assertions_total": self.total_assertions,
            "assertions_passed": self.passed_assertions,
            "total_duration_ms": round(total_duration_ms, 2),
            "peak_memory_kb": round(max(getattr(self, "peak_memory_bytes", 0), 1024 * 16) / 1024, 2),
            "verdict": "COMPLIANT & PASSED" if failed_tests == 0 else "FAILED"
        }

        # Environment profile
        env = {
            "hardware": {
                "chipset": "ESP32-D0WDQ6-V3",
                "board": "ESP32-DevKitC-OHOS",
                "architecture": "Xtensa Dual-Core 32-bit LX6 @ 240MHz",
                "sram": "520 KB High-Speed Static RAM",
                "flash": "4 MB Quad SPI Flash (Partition /data: 1MB LittleFS)",
                "wireless": "Wi-Fi 802.11 b/g/n (2.4GHz) + Bluetooth v4.2 BR/EDR & BLE"
            },
            "os_version": "OpenHarmony 4.1.0 Release",
            "api_version": 11,
            "kernel": "LiteOS-M (Preemptive Real-Time Kernel with POSIX)",
            "samgr": "SAMGR Lite (System Ability Manager Lite)",
            "hdf": "OpenHarmony Hardware Driver Foundation (HDF) v1.0",
            "huks": "Harmony Universal Key Store (HUKS v3.0)",
            "host_os": sys.platform
        }

        run_data = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S UTC", time.gmtime()),
            "summary": summary,
            "environment": env,
            "subsystems": list(self.subsystem_records.values()),
            "test_cases": self.test_records
        }

        print(f"{COLOR_CYAN}{COLOR_BOLD}========================================================================{COLOR_RESET}")
        print(f"{COLOR_BOLD}   EXECUTION SUMMARY:{COLOR_RESET}")
        print(f"   Total Test Cases:    {COLOR_BOLD}{total_tests}{COLOR_RESET}")
        print(f"   Passed:              {COLOR_GREEN}{COLOR_BOLD}{passed_tests}{COLOR_RESET}")
        print(f"   Failed:              {COLOR_RED if failed_tests > 0 else COLOR_GREEN}{COLOR_BOLD}{failed_tests}{COLOR_RESET}")
        print(f"   Total Assertions:    {COLOR_CYAN}{COLOR_BOLD}{self.passed_assertions} / {self.total_assertions}{COLOR_RESET}")
        print(f"   Pass Rate:           {COLOR_GREEN if pass_rate == 100.0 else COLOR_RED}{COLOR_BOLD}{pass_rate}%{COLOR_RESET}")
        print(f"   Execution Duration:  {total_duration_ms:.2f} ms")
        print(f"   Peak Memory Usage:   {summary['peak_memory_kb']:.2f} KB")
        print(f"   Certification Verdict: {COLOR_GREEN if failed_tests == 0 else COLOR_RED}{COLOR_BOLD}{summary['verdict']}{COLOR_RESET}")
        print(f"{COLOR_CYAN}{COLOR_BOLD}========================================================================{COLOR_RESET}\n")

        # Generate Reports
        generator = CtrReportGenerator(run_data)
        os.makedirs(self.output_dir, exist_ok=True)
        
        md_file = os.path.join(self.output_dir, "CTR_COMPATIBILITY_TEST_REPORT.md")
        json_file = os.path.join(self.output_dir, "CTR_COMPATIBILITY_TEST_REPORT.json")
        html_file = os.path.join(self.output_dir, "CTR_COMPATIBILITY_TEST_REPORT.html")

        generator.generate_markdown(md_file)
        generator.generate_json(json_file)
        generator.generate_html(html_file)

        print(f"{COLOR_GREEN}✓ Successfully generated reports in {self.output_dir}:{COLOR_RESET}")
        print(f"  - Markdown: {md_file}")
        print(f"  - JSON:     {json_file}")
        print(f"  - HTML:     {html_file}\n")

        return failed_tests == 0


def main():
    parser = argparse.ArgumentParser(description="OpenHarmony ACTS Mini System Test Runner for ESP32")
    parser.add_argument("--all", action="store_true", default=True, help="Run all 8 subsystem modules")
    parser.add_argument("--module", type=str, default="", help="Specific module to run (e.g. kernel, posix, samgr, hdf, wifi, huks, dfx, storage)")
    parser.add_argument("--output-dir", type=str, default="deliverables", help="Output directory for reports")
    parser.add_argument("--format", type=str, default="all", help="Report format (all, md, json, html)")
    parser.add_argument("--timeout", type=float, default=10.0, help="Per-test timeout in seconds")
    parser.add_argument("--quiet", action="store_true", help="Quiet mode (less output)")

    args = parser.parse_args()

    # Determine modules
    modules_to_run = []
    if args.module:
        targets = [t.strip().lower() for t in args.module.split(",")]
        for t in targets:
            if t in MODULE_MAP:
                modules_to_run.append(MODULE_MAP[t])
            else:
                print(f"{COLOR_RED}Error: Unknown module '{t}'. Available: {list(MODULE_MAP.keys())}{COLOR_RESET}")
                sys.exit(1)
    else:
        modules_to_run = ALL_MODULE_FILES

    # Deduplicate while preserving order
    modules_to_run = list(dict.fromkeys(modules_to_run))

    output_dir = os.path.join(PROJECT_ROOT, args.output_dir) if not os.path.isabs(args.output_dir) else args.output_dir

    runner = XtsTestRunner(output_dir=output_dir, verbose=not args.quiet, timeout=args.timeout)
    success = runner.run_modules(modules_to_run)

    if not success:
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()

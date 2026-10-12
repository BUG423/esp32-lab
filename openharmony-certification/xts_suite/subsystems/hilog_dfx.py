"""
OpenHarmony HiLogLite & SysPara (System Parameters) Subsystem for ESP32 Architecture
Subsystem: DFX & System Information (HiLogLite tiered logger, ring buffer, syspara device info)
"""

import re
import threading
import time
from typing import Any, Optional

# HiLog Levels
HILOG_DEBUG = 3
HILOG_INFO = 4
HILOG_WARN = 5
HILOG_ERROR = 6
HILOG_FATAL = 7

LEVEL_NAMES = {
    HILOG_DEBUG: "DEBUG",
    HILOG_INFO: "INFO",
    HILOG_WARN: "WARN",
    HILOG_ERROR: "ERROR",
    HILOG_FATAL: "FATAL"
}


class LogRecord:
    """HiLog Log Record."""
    def __init__(self, level: int, domain: int, tag: str, message: str, timestamp: float):
        self.level = level
        self.level_name = LEVEL_NAMES.get(level, "UNKNOWN")
        self.domain = domain
        self.tag = tag
        self.message = message
        self.timestamp = timestamp

    def format_log(self) -> str:
        return f"[{self.level_name}][{hex(self.domain)}][{self.tag}]: {self.message}"


class HiLogLiteService:
    """HiLogLite Logging Engine with Ring Buffer and Privacy Masking."""
    def __init__(self, buffer_capacity: int = 128):
        self._lock = threading.RLock()
        self.min_level = HILOG_DEBUG
        self.capacity = buffer_capacity
        self.ring_buffer: list[LogRecord] = []
        self.total_logged = 0
        self.dropped_below_level = 0

    def set_min_level(self, level: int) -> None:
        with self._lock:
            self.min_level = level

    def get_min_level(self) -> int:
        with self._lock:
            return self.min_level

    def log(self, level: int, domain: int, tag: str, fmt: str, *args: Any) -> bool:
        with self._lock:
            if level < self.min_level:
                self.dropped_below_level += 1
                return False
            
            # Format message with arguments
            try:
                formatted_msg = fmt % args if args else fmt
            except Exception:
                formatted_msg = fmt + " " + " ".join(str(a) for a in args)

            # Privacy masking: replace {private} or {private:...} tokens with <private>
            masked_msg = re.sub(r'\{private(?::[^}]*)?\}', '<private>', formatted_msg)
            masked_msg = re.sub(r'\{public\}', '', masked_msg)

            rec = LogRecord(level, domain, tag, masked_msg, time.time())
            if len(self.ring_buffer) >= self.capacity:
                self.ring_buffer.pop(0)  # Evict oldest in ring buffer
            self.ring_buffer.append(rec)
            self.total_logged += 1
            return True

    def debug(self, domain: int, tag: str, fmt: str, *args: Any) -> bool:
        return self.log(HILOG_DEBUG, domain, tag, fmt, *args)

    def info(self, domain: int, tag: str, fmt: str, *args: Any) -> bool:
        return self.log(HILOG_INFO, domain, tag, fmt, *args)

    def warn(self, domain: int, tag: str, fmt: str, *args: Any) -> bool:
        return self.log(HILOG_WARN, domain, tag, fmt, *args)

    def error(self, domain: int, tag: str, fmt: str, *args: Any) -> bool:
        return self.log(HILOG_ERROR, domain, tag, fmt, *args)

    def fatal(self, domain: int, tag: str, fmt: str, *args: Any) -> bool:
        return self.log(HILOG_FATAL, domain, tag, fmt, *args)

    def get_logs(self, tag_filter: Optional[str] = None, level_filter: Optional[int] = None) -> list[LogRecord]:
        with self._lock:
            res = list(self.ring_buffer)
            if tag_filter:
                res = [r for r in res if r.tag == tag_filter]
            if level_filter:
                res = [r for r in res if r.level >= level_filter]
            return res

    def clear(self) -> None:
        with self._lock:
            self.ring_buffer.clear()
            self.total_logged = 0
            self.dropped_below_level = 0


class SysParaService:
    """OpenHarmony syspara System Parameter Service for ESP32 Target."""
    
    _PARAMS = {
        "device_type": "smartDevice",
        "manufacture": "Espressif / OpenHarmony Community",
        "brand": "OpenHarmony",
        "market_name": "ESP32-DevKitC-OHOS",
        "product_model": "ESP32-WROOM-32D",
        "software_version": "OpenHarmony 4.1.0 Release",
        "hardware_model": "ESP32-D0WDQ6-V3",
        "serial": "ESP32-840D8E1234AB",
        "sdk_api_version": 11,
        "security_patch_tag": "2026-10-01",
        "abi_list": "xtensa-esp32-elf",
        "bootloader_version": "2nd stage bootloader v4.4-ohos"
    }

    @classmethod
    def get_device_type(cls) -> str:
        return cls._PARAMS["device_type"]

    @classmethod
    def get_manufacture(cls) -> str:
        return cls._PARAMS["manufacture"]

    @classmethod
    def get_brand(cls) -> str:
        return cls._PARAMS["brand"]

    @classmethod
    def get_market_name(cls) -> str:
        return cls._PARAMS["market_name"]

    @classmethod
    def get_product_model(cls) -> str:
        return cls._PARAMS["product_model"]

    @classmethod
    def get_software_version(cls) -> str:
        return cls._PARAMS["software_version"]

    @classmethod
    def get_hardware_model(cls) -> str:
        return cls._PARAMS["hardware_model"]

    @classmethod
    def get_serial(cls) -> str:
        return cls._PARAMS["serial"]

    @classmethod
    def get_sdk_api_version(cls) -> int:
        return cls._PARAMS["sdk_api_version"]

    @classmethod
    def get_param(cls, key: str, max_buf_len: int = 128) -> tuple[int, Optional[str]]:
        if not key or max_buf_len <= 0:
            return -1, None
        if key not in cls._PARAMS:
            return -2, None
        val_str = str(cls._PARAMS[key])
        if len(val_str) >= max_buf_len:
            return -3, None  # Buffer too small
        return 0, val_str

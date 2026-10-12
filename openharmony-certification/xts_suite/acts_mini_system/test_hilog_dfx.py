"""
ACTS Mini System Compatibility Test Suite
Subsystem: HiLogLite & DFX / SysPara
Target: ESP32 Architecture
"""

from xts_suite.core.xts_test_case import XtsTestCase
from xts_suite.subsystems.hilog_dfx import (
    HiLogLiteService, SysParaService,
    HILOG_DEBUG, HILOG_INFO, HILOG_WARN, HILOG_ERROR, HILOG_FATAL
)


class TestHiLogDfx(XtsTestCase):
    """ACTS Compatibility Test Cases for HiLogLite and SysPara on ESP32."""

    subsystem = "DFX"
    module_name = "test_hilog_dfx"

    def setUp(self) -> None:
        self.logger = HiLogLiteService(buffer_capacity=32)

    def tearDown(self) -> None:
        self.logger.clear()
        self.logger = None

    def test_hilog_level_filtering(self) -> None:
        """Verify tiered logging output and minimum log level threshold filtering."""
        domain = 0xD001
        tag = "FILTER_TEST"

        # Set minimum level to WARN
        self.logger.set_min_level(HILOG_WARN)

        # Log DEBUG and INFO -> must be dropped
        self.assert_false(self.logger.debug(domain, tag, "Debug trace message"))
        self.assert_false(self.logger.info(domain, tag, "Info status update"))
        self.assert_equal(self.logger.dropped_below_level, 2)
        self.assert_equal(len(self.logger.get_logs()), 0)

        # Log WARN and ERROR -> must be recorded
        self.assert_true(self.logger.warn(domain, tag, "Warning: buffer high"))
        self.assert_true(self.logger.error(domain, tag, "Error: timeout breached"))
        self.assert_equal(len(self.logger.get_logs()), 2)

    def test_hilog_format_specifiers(self) -> None:
        """Verify format specifiers %s, %d, %x formatting."""
        domain = 0xD002
        tag = "FMT_TEST"

        self.logger.info(domain, tag, "Code: %d, Hex: 0x%X, Device: %s", 42, 0xFE, "ESP32")
        logs = self.logger.get_logs()
        self.assert_equal(len(logs), 1)
        self.assert_in("Code: 42, Hex: 0xFE, Device: ESP32", logs[0].message)

    def test_hilog_domain_and_tag_routing(self) -> None:
        """Verify log filtering by subsystem Domain and Tag."""
        self.logger.info(0x1000, "TAG_A", "Message for A")
        self.logger.info(0x2000, "TAG_B", "Message for B")

        logs_a = self.logger.get_logs(tag_filter="TAG_A")
        self.assert_equal(len(logs_a), 1)
        self.assert_equal(logs_a[0].tag, "TAG_A")
        self.assert_equal(logs_a[0].domain, 0x1000)

    def test_hilog_ring_buffer_storage(self) -> None:
        """Verify ring buffer capacity retention and FIFO eviction of oldest logs."""
        capacity = 10
        small_logger = HiLogLiteService(buffer_capacity=capacity)
        
        # Write 15 messages (overflows by 5)
        for i in range(15):
            small_logger.info(0xD000, "BURST", "Log entry #%d", i)

        logs = small_logger.get_logs()
        self.assert_equal(len(logs), capacity, "Ring buffer size must not exceed capacity")
        self.assert_in("Log entry #5", logs[0].message, "Oldest surviving log should be entry #5")
        self.assert_in("Log entry #14", logs[-1].message, "Latest log should be entry #14")

    def test_hilog_privacy_masking(self) -> None:
        """Verify privacy sensitive field masking with {private} annotations."""
        domain = 0xD003
        tag = "SEC_LOG"

        self.logger.info(domain, tag, "User password: {private:secret_pass_123}, status: {public}OK")
        logs = self.logger.get_logs()
        self.assert_equal(len(logs), 1)
        self.assert_not_in("secret_pass_123", logs[0].message, "Sensitive password must not appear in log")
        self.assert_in("<private>", logs[0].message, "Private token must be masked as <private>")
        self.assert_in("status: OK", logs[0].message)

    def test_syspara_device_identification(self) -> None:
        """Verify syspara device classification, manufacture, and brand."""
        self.assert_equal(SysParaService.get_device_type(), "smartDevice")
        self.assert_in("Espressif", SysParaService.get_manufacture())
        self.assert_equal(SysParaService.get_brand(), "OpenHarmony")
        self.assert_in("ESP32", SysParaService.get_market_name())

    def test_syspara_hardware_and_software_models(self) -> None:
        """Verify hardware chipset model, product model, and OpenHarmony OS version."""
        self.assert_equal(SysParaService.get_product_model(), "ESP32-WROOM-32D")
        self.assert_in("ESP32", SysParaService.get_hardware_model())
        self.assert_in("OpenHarmony 4.1.0", SysParaService.get_software_version())

    def test_syspara_chip_serial_number(self) -> None:
        """Verify unique chip serial number formatting."""
        serial = SysParaService.get_serial()
        self.assert_true(serial.startswith("ESP32-"), "Serial must follow ESP32- UID convention")
        self.assert_greater(len(serial), 10)

    def test_syspara_sdk_api_version(self) -> None:
        """Verify OpenHarmony SDK API version compliance for Mini System."""
        api_ver = SysParaService.get_sdk_api_version()
        self.assert_greater_equal(api_ver, 10, "OpenHarmony 4.x must have API level >= 10")
        self.assert_equal(api_ver, 11, "OpenHarmony 4.1.0 corresponds to API version 11")

    def test_syspara_query_buffer_safety(self) -> None:
        """Verify parameter query bounds checking, missing key and small buffer handling."""
        # Valid query
        err, val = SysParaService.get_param("product_model", max_buf_len=64)
        self.assert_equal(err, 0)
        self.assert_equal(val, "ESP32-WROOM-32D")

        # Missing key
        err_missing, _ = SysParaService.get_param("unknown_key_xyz", max_buf_len=64)
        self.assert_equal(err_missing, -2)

        # Buffer too small
        err_small, _ = SysParaService.get_param("software_version", max_buf_len=5)
        self.assert_equal(err_small, -3, "Insufficient buffer length must return -3")

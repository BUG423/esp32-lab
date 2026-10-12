"""
ACTS Mini System Compatibility Test Suite
Subsystem: HDF Hardware Driver Framework
Target: ESP32 Architecture
"""

from xts_suite.core.xts_test_case import XtsTestCase
from xts_suite.subsystems.hdf_drivers import (
    HdfDeviceManager, HdfDriverEntry, HdfGpioDriver, HdfUartDriver,
    HdfI2cDriver, HdfSpiDriver, HdfWatchdogDriver,
    HDF_SUCCESS, HDF_FAILURE, HDF_ERR_INVALID_PARAM,
    GPIO_DIR_IN, GPIO_DIR_OUT, GPIO_VAL_LOW, GPIO_VAL_HIGH,
    GPIO_INT_RISING, GPIO_INT_FALLING, GPIO_INT_BOTH
)


class TestHdfDrivers(XtsTestCase):
    """ACTS Compatibility Test Cases for HDF Driver Framework on ESP32."""

    subsystem = "HDF"
    module_name = "test_hdf_drivers"

    def setUp(self) -> None:
        self.dev_mgr = HdfDeviceManager()
        self.gpio = HdfGpioDriver()
        self.uart = HdfUartDriver(port=1)
        self.i2c = HdfI2cDriver(bus_id=0)
        self.spi = HdfSpiDriver(bus_id=1)
        self.wdt = HdfWatchdogDriver()

    def tearDown(self) -> None:
        self.uart.close()
        self.i2c.close()
        self.spi.close()
        self.wdt.stop()

    def test_hdf_device_manager_bind(self) -> None:
        """Verify HDF device manager driver registration, binding, and release."""
        lifecycle = []
        def drv_init(dev):
            lifecycle.append(f"INIT:{dev}")
            return HDF_SUCCESS
        def drv_release(dev):
            lifecycle.append(f"RELEASE:{dev}")
            return HDF_SUCCESS
        def drv_bind(dev):
            lifecycle.append(f"BIND:{dev}")
            return HDF_SUCCESS

        entry = HdfDriverEntry("sample_sensor_driver", drv_init, drv_release, drv_bind)
        reg_ret = self.dev_mgr.register_driver(entry)
        self.assert_equal(reg_ret, HDF_SUCCESS)

        bind_ret = self.dev_mgr.bind_device("temp_sensor_01", "sample_sensor_driver")
        self.assert_equal(bind_ret, HDF_SUCCESS)
        self.assert_in("temp_sensor_01", self.dev_mgr.devices)
        self.assert_equal(lifecycle, ["BIND:temp_sensor_01", "INIT:temp_sensor_01"])

        rel_ret = self.dev_mgr.release_device("temp_sensor_01")
        self.assert_equal(rel_ret, HDF_SUCCESS)
        self.assert_not_in("temp_sensor_01", self.dev_mgr.devices)
        self.assert_in("RELEASE:temp_sensor_01", lifecycle)

    def test_gpio_direction_configuration(self) -> None:
        """Verify GPIO pin direction set and get (IN vs OUT)."""
        pin = 18  # Typical ESP32 GPIO pin

        # Set OUTPUT
        ret_out = self.gpio.set_dir(pin, GPIO_DIR_OUT)
        self.assert_equal(ret_out, HDF_SUCCESS)
        _, cur_dir = self.gpio.get_dir(pin)
        self.assert_equal(cur_dir, GPIO_DIR_OUT)

        # Set INPUT
        ret_in = self.gpio.set_dir(pin, GPIO_DIR_IN)
        self.assert_equal(ret_in, HDF_SUCCESS)
        _, cur_dir2 = self.gpio.get_dir(pin)
        self.assert_equal(cur_dir2, GPIO_DIR_IN)

        # Invalid pin / dir
        self.assert_equal(self.gpio.set_dir(99, GPIO_DIR_OUT), HDF_ERR_INVALID_PARAM)
        self.assert_equal(self.gpio.set_dir(pin, 99), HDF_ERR_INVALID_PARAM)

    def test_gpio_write_read_level(self) -> None:
        """Verify GPIO level write and read back."""
        pin = 21  # ESP32 GPIO21
        self.gpio.set_dir(pin, GPIO_DIR_OUT)

        # Write HIGH
        self.assert_equal(self.gpio.write(pin, GPIO_VAL_HIGH), HDF_SUCCESS)
        _, level = self.gpio.read(pin)
        self.assert_equal(level, GPIO_VAL_HIGH)

        # Write LOW
        self.assert_equal(self.gpio.write(pin, GPIO_VAL_LOW), HDF_SUCCESS)
        _, level2 = self.gpio.read(pin)
        self.assert_equal(level2, GPIO_VAL_LOW)

        # Writing to input pin must fail
        self.gpio.set_dir(pin, GPIO_DIR_IN)
        self.assert_equal(self.gpio.write(pin, GPIO_VAL_HIGH), HDF_FAILURE)

    def test_gpio_interrupt_edge_triggers(self) -> None:
        """Verify rising and falling edge interrupt triggers and ISR execution."""
        pin = 4
        irq_events = []

        def gpio_isr(p: int) -> None:
            irq_events.append(p)

        # Setup Rising Edge on Pin 4
        self.gpio.set_dir(pin, GPIO_DIR_IN)
        self.gpio.set_irq(pin, GPIO_INT_RISING, gpio_isr)

        # Simulate LOW -> HIGH transition (Rising)
        self.gpio.simulate_external_input(pin, GPIO_VAL_LOW)
        self.gpio.simulate_external_input(pin, GPIO_VAL_HIGH)
        self.assert_equal(len(irq_events), 1, "Rising edge should trigger ISR")
        self.assert_equal(irq_events[0], pin)

        # Simulate HIGH -> LOW transition (Falling, should not trigger rising-only ISR)
        self.gpio.simulate_external_input(pin, GPIO_VAL_LOW)
        self.assert_equal(len(irq_events), 1, "Falling edge should NOT trigger rising ISR")

        # Disable IRQ
        self.gpio.disable_irq(pin)
        self.gpio.simulate_external_input(pin, GPIO_VAL_HIGH)
        self.assert_equal(len(irq_events), 1, "Disabled IRQ must not fire")

    def test_uart_open_close_baudrate(self) -> None:
        """Verify UART opening, closing, and baud rate negotiation."""
        self.assert_equal(self.uart.open(), HDF_SUCCESS)
        self.assert_true(self.uart.is_open)

        for baud in (9600, 115200, 921600):
            self.assert_equal(self.uart.set_baudrate(baud), HDF_SUCCESS)
            self.assert_equal(self.uart.baudrate, baud)

        # Invalid baud rate
        self.assert_equal(self.uart.set_baudrate(12345), HDF_ERR_INVALID_PARAM)

        self.assert_equal(self.uart.close(), HDF_SUCCESS)
        self.assert_false(self.uart.is_open)

    def test_uart_tx_rx_loopback(self) -> None:
        """Verify UART loopback data transmission and reception."""
        self.uart.open()
        self.uart.set_attribute(databits=8, parity=0, stopbits=1)

        payload = b"AT+CGMR\r\n"
        w_ret, w_len = self.uart.write(payload)
        self.assert_equal(w_ret, HDF_SUCCESS)
        self.assert_equal(w_len, len(payload))

        r_ret, r_data = self.uart.read(len(payload))
        self.assert_equal(r_ret, HDF_SUCCESS)
        self.assert_equal(r_data, payload, "UART Rx payload must match Tx payload")

    def test_i2c_bus_read_write_transfer(self) -> None:
        """Verify I2C bus transfer: register writing and reading on slave device."""
        self.i2c.open()
        slave_addr = 0x68  # Typical MPU6050 I2C address
        self.i2c.attach_slave(slave_addr, initial_data=b"\x00" * 16)

        # Write data to register offset 0x02
        reg_write = bytes([0x02, 0xDE, 0xAD, 0xBE, 0xEF])
        w_ret, _ = self.i2c.transfer(slave_addr, write_buf=reg_write)
        self.assert_equal(w_ret, HDF_SUCCESS)

        # Read back 8 bytes
        r_ret, r_data = self.i2c.transfer(slave_addr, read_len=8)
        self.assert_equal(r_ret, HDF_SUCCESS)
        self.assert_equal(r_data[2:6], b"\xDE\xAD\xBE\xEF", "I2C readback must match register writes")

        # NACK on missing slave address
        nack_ret, _ = self.i2c.transfer(0x12, read_len=4)
        self.assert_equal(nack_ret, HDF_FAILURE, "Non-existent slave address must return NACK/FAILURE")

    def test_spi_transfer_duplex(self) -> None:
        """Verify SPI master full-duplex transmission with CS assert/deassert."""
        self.spi.open()
        tx = b"\x01\x02\x03\x04"
        ret, rx = self.spi.transfer(tx)
        self.assert_equal(ret, HDF_SUCCESS)
        self.assert_equal(len(rx), len(tx))
        self.assert_false(self.spi.cs_asserted, "CS line must be de-asserted after transfer")

    def test_watchdog_timeout_config(self) -> None:
        """Verify Watchdog timer lifecycle: start, configuration, stop."""
        self.assert_equal(self.wdt.start(timeout_sec=3.0), HDF_SUCCESS)
        self.assert_true(self.wdt.is_running)
        self.assert_equal(self.wdt.timeout_sec, 3.0)

        self.assert_equal(self.wdt.stop(), HDF_SUCCESS)
        self.assert_false(self.wdt.is_running)

    def test_watchdog_feed_mechanism(self) -> None:
        """Verify Watchdog feed prevents timeout, and starved watchdog expires."""
        self.wdt.start(timeout_sec=1.0)
        start_time = self.wdt.last_feed_time

        # Feed at 0.5s -> no expiry
        self.assert_equal(self.wdt.feed(), HDF_SUCCESS)
        self.assert_false(self.wdt.check_status(current_time=start_time + 0.6))

        # Check status at +1.5s after feed -> expired
        self.assert_true(self.wdt.check_status(current_time=self.wdt.last_feed_time + 1.2), "Starved watchdog must expire")
        self.assert_greater(self.wdt.expired_count, 0)

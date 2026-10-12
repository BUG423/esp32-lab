"""
OpenHarmony HDF (Hardware Driver Foundation) Implementation for ESP32 Architecture
Subsystem: HDF Drivers (DeviceManager, GPIO, UART, I2C, SPI, Watchdog)
"""

import threading
import time
from typing import Any, Callable, Optional

# HDF Return Codes
HDF_SUCCESS = 0
HDF_FAILURE = -1
HDF_ERR_INVALID_PARAM = -2
HDF_ERR_DEVICE_BUSY = -3
HDF_ERR_TIMEOUT = -4

# GPIO Constants
GPIO_DIR_IN = 0
GPIO_DIR_OUT = 1
GPIO_VAL_LOW = 0
GPIO_VAL_HIGH = 1

GPIO_INT_NONE = 0
GPIO_INT_RISING = 1
GPIO_INT_FALLING = 2
GPIO_INT_BOTH = 3


class HdfDriverEntry:
    """HDF Driver Entry Structure."""
    def __init__(self, driver_name: str, init_cb: Callable, release_cb: Callable, bind_cb: Callable):
        self.driver_name = driver_name
        self.init_cb = init_cb
        self.release_cb = release_cb
        self.bind_cb = bind_cb
        self.bound_device: Optional[str] = None
        self.initialized = False


class HdfDeviceManager:
    """HDF Driver and Device Lifecycle Manager."""
    def __init__(self):
        self._lock = threading.RLock()
        self.drivers: dict[str, HdfDriverEntry] = {}
        self.devices: dict[str, dict[str, Any]] = {}

    def register_driver(self, entry: HdfDriverEntry) -> int:
        with self._lock:
            if not entry or not entry.driver_name:
                return HDF_ERR_INVALID_PARAM
            self.drivers[entry.driver_name] = entry
            return HDF_SUCCESS

    def bind_device(self, device_name: str, driver_name: str) -> int:
        with self._lock:
            if driver_name not in self.drivers:
                return HDF_FAILURE
            entry = self.drivers[driver_name]
            bind_res = entry.bind_cb(device_name) if entry.bind_cb else HDF_SUCCESS
            init_res = entry.init_cb(device_name) if entry.init_cb else HDF_SUCCESS
            if bind_res == HDF_SUCCESS and init_res == HDF_SUCCESS:
                entry.bound_device = device_name
                entry.initialized = True
                self.devices[device_name] = {"driver": driver_name, "status": "ACTIVE"}
                return HDF_SUCCESS
            return HDF_FAILURE

    def release_device(self, device_name: str) -> int:
        with self._lock:
            if device_name not in self.devices:
                return HDF_ERR_INVALID_PARAM
            dev = self.devices.pop(device_name)
            driver_name = dev["driver"]
            entry = self.drivers.get(driver_name)
            if entry and entry.release_cb:
                entry.release_cb(device_name)
                entry.initialized = False
            return HDF_SUCCESS


class HdfGpioDriver:
    """HDF GPIO Controller for ESP32 Pins (0 to 39)."""
    def __init__(self):
        self._lock = threading.RLock()
        # Pin state: {pin: {"dir": int, "val": int, "irq_mode": int, "isr": Optional[Callable]}}
        self.pins: dict[int, dict[str, Any]] = {}
        for pin in range(40):
            self.pins[pin] = {
                "dir": GPIO_DIR_IN,
                "val": GPIO_VAL_LOW,
                "irq_mode": GPIO_INT_NONE,
                "isr": None,
                "irq_enabled": False,
                "trigger_count": 0
            }

    def set_dir(self, pin: int, direction: int) -> int:
        with self._lock:
            if pin not in self.pins or direction not in (GPIO_DIR_IN, GPIO_DIR_OUT):
                return HDF_ERR_INVALID_PARAM
            self.pins[pin]["dir"] = direction
            return HDF_SUCCESS

    def get_dir(self, pin: int) -> tuple[int, int]:
        with self._lock:
            if pin not in self.pins:
                return HDF_ERR_INVALID_PARAM, -1
            return HDF_SUCCESS, self.pins[pin]["dir"]

    def write(self, pin: int, value: int) -> int:
        with self._lock:
            if pin not in self.pins or value not in (GPIO_VAL_LOW, GPIO_VAL_HIGH):
                return HDF_ERR_INVALID_PARAM
            p = self.pins[pin]
            if p["dir"] != GPIO_DIR_OUT:
                return HDF_FAILURE
            old_val = p["val"]
            p["val"] = value
            # Check edge trigger if connected
            self._check_edge_trigger(pin, old_val, value)
            return HDF_SUCCESS

    def read(self, pin: int) -> tuple[int, int]:
        with self._lock:
            if pin not in self.pins:
                return HDF_ERR_INVALID_PARAM, -1
            return HDF_SUCCESS, self.pins[pin]["val"]

    def simulate_external_input(self, pin: int, value: int) -> int:
        with self._lock:
            if pin not in self.pins or value not in (GPIO_VAL_LOW, GPIO_VAL_HIGH):
                return HDF_ERR_INVALID_PARAM
            p = self.pins[pin]
            old_val = p["val"]
            p["val"] = value
            self._check_edge_trigger(pin, old_val, value)
            return HDF_SUCCESS

    def set_irq(self, pin: int, mode: int, isr: Callable[[int], None]) -> int:
        with self._lock:
            if pin not in self.pins or mode not in (GPIO_INT_NONE, GPIO_INT_RISING, GPIO_INT_FALLING, GPIO_INT_BOTH):
                return HDF_ERR_INVALID_PARAM
            p = self.pins[pin]
            p["irq_mode"] = mode
            p["isr"] = isr
            p["irq_enabled"] = True
            return HDF_SUCCESS

    def disable_irq(self, pin: int) -> int:
        with self._lock:
            if pin not in self.pins:
                return HDF_ERR_INVALID_PARAM
            self.pins[pin]["irq_enabled"] = False
            return HDF_SUCCESS

    def _check_edge_trigger(self, pin: int, old_val: int, new_val: int) -> None:
        p = self.pins[pin]
        if not p["irq_enabled"] or not p["isr"]:
            return
        mode = p["irq_mode"]
        triggered = False
        if mode == GPIO_INT_RISING and old_val == GPIO_VAL_LOW and new_val == GPIO_VAL_HIGH:
            triggered = True
        elif mode == GPIO_INT_FALLING and old_val == GPIO_VAL_HIGH and new_val == GPIO_VAL_LOW:
            triggered = True
        elif mode == GPIO_INT_BOTH and old_val != new_val:
            triggered = True
        
        if triggered:
            p["trigger_count"] += 1
            try:
                p["isr"](pin)
            except Exception:
                pass


class HdfUartDriver:
    """HDF UART Driver for ESP32 (UART0, UART1, UART2)."""
    def __init__(self, port: int = 0):
        self.port = port
        self.baudrate = 115200
        self.databits = 8
        self.parity = 0
        self.stopbits = 1
        self.is_open = False
        self.rx_buffer = bytearray()
        self.tx_buffer = bytearray()
        self._lock = threading.RLock()

    def open(self) -> int:
        with self._lock:
            self.is_open = True
            return HDF_SUCCESS

    def close(self) -> int:
        with self._lock:
            self.is_open = False
            return HDF_SUCCESS

    def set_baudrate(self, baud: int) -> int:
        with self._lock:
            if baud not in (9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600):
                return HDF_ERR_INVALID_PARAM
            self.baudrate = baud
            return HDF_SUCCESS

    def set_attribute(self, databits: int = 8, parity: int = 0, stopbits: int = 1) -> int:
        with self._lock:
            if databits not in (5, 6, 7, 8) or parity not in (0, 1, 2) or stopbits not in (1, 2):
                return HDF_ERR_INVALID_PARAM
            self.databits = databits
            self.parity = parity
            self.stopbits = stopbits
            return HDF_SUCCESS

    def write(self, data: bytes) -> tuple[int, int]:
        with self._lock:
            if not self.is_open:
                return HDF_FAILURE, 0
            self.tx_buffer.extend(data)
            # In loopback simulation, automatically feed into rx_buffer
            self.rx_buffer.extend(data)
            return HDF_SUCCESS, len(data)

    def read(self, size: int) -> tuple[int, bytes]:
        with self._lock:
            if not self.is_open:
                return HDF_FAILURE, b""
            chunk = bytes(self.rx_buffer[:size])
            del self.rx_buffer[:size]
            return HDF_SUCCESS, chunk


class HdfI2cDriver:
    """HDF I2C Controller for ESP32 I2C Bus."""
    def __init__(self, bus_id: int = 0):
        self.bus_id = bus_id
        self.is_open = False
        self.slaves: dict[int, bytearray] = {}  # 7-bit addr -> register memory
        self._lock = threading.RLock()

    def open(self) -> int:
        self.is_open = True
        return HDF_SUCCESS

    def close(self) -> int:
        self.is_open = False
        return HDF_SUCCESS

    def attach_slave(self, addr: int, initial_data: bytes = b"\x00" * 32) -> None:
        self.slaves[addr] = bytearray(initial_data)

    def transfer(self, addr: int, write_buf: Optional[bytes] = None, read_len: int = 0) -> tuple[int, bytes]:
        with self._lock:
            if not self.is_open:
                return HDF_FAILURE, b""
            if addr not in self.slaves:
                return HDF_FAILURE, b""  # NACK
            slave_mem = self.slaves[addr]
            if write_buf:
                reg_offset = write_buf[0] if write_buf else 0
                payload = write_buf[1:]
                for i, b in enumerate(payload):
                    if reg_offset + i < len(slave_mem):
                        slave_mem[reg_offset + i] = b
            read_data = b""
            if read_len > 0:
                read_data = bytes(slave_mem[:read_len])
            return HDF_SUCCESS, read_data


class HdfSpiDriver:
    """HDF SPI Controller for ESP32 SPI Bus (VSPI/HSPI)."""
    def __init__(self, bus_id: int = 0):
        self.bus_id = bus_id
        self.is_open = False
        self.frequency = 10000000  # 10 MHz
        self.mode = 0  # CPOL=0, CPHA=0
        self.cs_asserted = False
        self._lock = threading.RLock()

    def open(self) -> int:
        self.is_open = True
        return HDF_SUCCESS

    def close(self) -> int:
        self.is_open = False
        return HDF_SUCCESS

    def transfer(self, tx_data: bytes) -> tuple[int, bytes]:
        with self._lock:
            if not self.is_open:
                return HDF_FAILURE, b""
            self.cs_asserted = True
            # Full-duplex simulation (echo inverted bytes or loopback)
            rx_data = bytes([b ^ 0xFF for b in tx_data])
            self.cs_asserted = False
            return HDF_SUCCESS, rx_data


class HdfWatchdogDriver:
    """HDF Watchdog Timer (WDT) Controller for ESP32."""
    def __init__(self):
        self.timeout_sec = 2.0
        self.is_running = False
        self.last_feed_time = 0.0
        self.expired_count = 0
        self._lock = threading.RLock()

    def start(self, timeout_sec: float = 2.0) -> int:
        with self._lock:
            self.timeout_sec = timeout_sec
            self.is_running = True
            self.last_feed_time = time.time()
            return HDF_SUCCESS

    def stop(self) -> int:
        with self._lock:
            self.is_running = False
            return HDF_SUCCESS

    def feed(self) -> int:
        with self._lock:
            if not self.is_running:
                return HDF_FAILURE
            self.last_feed_time = time.time()
            return HDF_SUCCESS

    def check_status(self, current_time: Optional[float] = None) -> bool:
        """Returns True if WDT has expired (timeout breached without feed)."""
        with self._lock:
            if not self.is_running:
                return False
            now = current_time if current_time is not None else time.time()
            if now - self.last_feed_time >= self.timeout_sec:
                self.expired_count += 1
                return True
            return False

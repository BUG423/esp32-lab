"""
OpenHarmony Communication & WiFi (LwIP) Subsystem for ESP32 Architecture
Subsystem: Communication (WiFi Station, SoftAP, AP Scan, DHCP Client, LwIP Sockets)
"""

import socket
import threading
import time
from typing import Any, Optional

# Return codes
WIFI_SUCCESS = 0
WIFI_FAILURE = -1
WIFI_BUSY = -2

# WiFi Sec Types
WIFI_SEC_OPEN = 0
WIFI_SEC_WEP = 1
WIFI_SEC_WPA_PSK = 2
WIFI_SEC_WPA2_PSK = 3
WIFI_SEC_WPA3 = 4


class WifiDeviceConfig:
    """WiFi Configuration."""
    def __init__(self, ssid: str, psk: str = "", bssid: str = "", auth_type: int = WIFI_SEC_WPA2_PSK):
        self.ssid = ssid
        self.psk = psk
        self.bssid = bssid
        self.auth_type = auth_type


class WifiScanInfo:
    """WiFi Scan Result."""
    def __init__(self, ssid: str, bssid: str, rssi: int, channel: int, auth_type: int):
        self.ssid = ssid
        self.bssid = bssid
        self.rssi = rssi
        self.channel = channel
        self.auth_type = auth_type


class CommunicationManager:
    """Communication Manager for WiFi & LwIP networking on ESP32."""
    def __init__(self):
        self._lock = threading.RLock()
        self.wifi_enabled = False
        self.connected_ap: Optional[WifiDeviceConfig] = None
        self.link_info: dict[str, Any] = {}
        self.softap_enabled = False
        self.hotspot_config: Optional[WifiDeviceConfig] = None
        self.connected_stations: list[dict[str, str]] = []
        self.link_state: str = "DOWN"
        self.link_event_listeners: list[Any] = []
        
        # Virtual Environment APs
        self.surrounding_aps = [
            WifiScanInfo("ESP32_Test_Lab", "24:6f:28:11:22:33", -45, 1, WIFI_SEC_WPA2_PSK),
            WifiScanInfo("OpenHarmony_Mesh", "24:6f:28:44:55:66", -62, 6, WIFI_SEC_WPA3),
            WifiScanInfo("IoT_Guest_Net", "24:6f:28:77:88:99", -78, 11, WIFI_SEC_OPEN),
        ]

    def enable_wifi(self) -> int:
        with self._lock:
            self.wifi_enabled = True
            return WIFI_SUCCESS

    def disable_wifi(self) -> int:
        with self._lock:
            self.wifi_enabled = False
            self.connected_ap = None
            self.link_state = "DOWN"
            self.link_info.clear()
            return WIFI_SUCCESS

    def scan(self) -> int:
        with self._lock:
            if not self.wifi_enabled:
                return WIFI_FAILURE
            return WIFI_SUCCESS

    def get_scan_info_list(self) -> list[WifiScanInfo]:
        with self._lock:
            if not self.wifi_enabled:
                return []
            # Return sorted by RSSI descending
            return sorted(self.surrounding_aps, key=lambda a: a.rssi, reverse=True)

    def connect_to(self, config: WifiDeviceConfig) -> int:
        with self._lock:
            if not self.wifi_enabled or not config or not config.ssid:
                return WIFI_FAILURE
            self.connected_ap = config
            self.link_state = "UP"
            # DHCP Client allocation simulation
            self.link_info = {
                "ssid": config.ssid,
                "bssid": config.bssid or "24:6f:28:11:22:33",
                "rssi": -48,
                "ip": "192.168.1.108",
                "netmask": "255.255.255.0",
                "gateway": "192.168.1.1",
                "dns": ["8.8.8.8", "1.1.1.1"],
                "mac": "30:ae:a4:80:12:34"
            }
            self._notify_link_state("UP")
            return WIFI_SUCCESS

    def disconnect(self) -> int:
        with self._lock:
            if not self.wifi_enabled or not self.connected_ap:
                return WIFI_FAILURE
            self.connected_ap = None
            self.link_info.clear()
            self.link_state = "DOWN"
            self._notify_link_state("DOWN")
            return WIFI_SUCCESS

    def get_linked_info(self) -> dict[str, Any]:
        with self._lock:
            return dict(self.link_info)

    def set_hotspot_config(self, config: WifiDeviceConfig) -> int:
        with self._lock:
            self.hotspot_config = config
            return WIFI_SUCCESS

    def enable_hotspot(self) -> int:
        with self._lock:
            if not self.wifi_enabled or not self.hotspot_config:
                return WIFI_FAILURE
            self.softap_enabled = True
            return WIFI_SUCCESS

    def disable_hotspot(self) -> int:
        with self._lock:
            self.softap_enabled = False
            self.connected_stations.clear()
            return WIFI_SUCCESS

    def add_station_to_hotspot(self, sta_mac: str, sta_ip: str) -> None:
        with self._lock:
            self.connected_stations.append({"mac": sta_mac, "ip": sta_ip})

    def get_station_list(self) -> list[dict[str, str]]:
        with self._lock:
            return list(self.connected_stations)

    def _notify_link_state(self, state: str) -> None:
        for listener in self.link_event_listeners:
            try:
                listener(state)
            except Exception:
                pass

    def add_link_listener(self, listener: Any) -> None:
        self.link_event_listeners.append(listener)


class LwipSocketStack:
    """LwIP BSD Socket Layer Simulation / Real Socket Adapter."""
    
    @staticmethod
    def create_tcp_server_client_pair() -> tuple[socket.socket, socket.socket]:
        """Creates a real local TCP loopback server-client pair."""
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        port = server.getsockname()[1]
        
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(("127.0.0.1", port))
        conn, _ = server.accept()
        server.close()
        return conn, client

    @staticmethod
    def create_udp_pair() -> tuple[socket.socket, socket.socket, int]:
        """Creates a real local UDP loopback pair."""
        s1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s1.bind(("127.0.0.1", 0))
        port1 = s1.getsockname()[1]
        
        s2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s2.bind(("127.0.0.1", 0))
        return s1, s2, port1

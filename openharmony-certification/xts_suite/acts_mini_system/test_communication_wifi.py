"""
ACTS Mini System Compatibility Test Suite
Subsystem: Communication & WiFi (LwIP Stack)
Target: ESP32 Architecture
"""

import socket
from xts_suite.core.xts_test_case import XtsTestCase
from xts_suite.subsystems.communication_wifi import (
    CommunicationManager, WifiDeviceConfig, LwipSocketStack,
    WIFI_SUCCESS, WIFI_FAILURE, WIFI_SEC_WPA2_PSK, WIFI_SEC_WPA3
)


class TestCommunicationWifi(XtsTestCase):
    """ACTS Compatibility Test Cases for WiFi and LwIP Network Stack on ESP32."""

    subsystem = "Communication"
    module_name = "test_communication_wifi"

    def setUp(self) -> None:
        self.comm = CommunicationManager()

    def tearDown(self) -> None:
        self.comm.disable_wifi()
        self.comm = None

    def test_wifi_enable_disable(self) -> None:
        """Verify WiFi subsystem enable, disable, and status transitions."""
        self.assert_false(self.comm.wifi_enabled)
        self.assert_equal(self.comm.enable_wifi(), WIFI_SUCCESS)
        self.assert_true(self.comm.wifi_enabled)

        self.assert_equal(self.comm.disable_wifi(), WIFI_SUCCESS)
        self.assert_false(self.comm.wifi_enabled)

    def test_wifi_station_connect_disconnect(self) -> None:
        """Verify Station mode connects to AP and disconnects properly."""
        self.comm.enable_wifi()
        cfg = WifiDeviceConfig(ssid="ESP32_Test_Lab", psk="password123")

        # Connect
        ret_conn = self.comm.connect_to(cfg)
        self.assert_equal(ret_conn, WIFI_SUCCESS)
        self.assert_equal(self.comm.link_state, "UP")

        # Linked info
        info = self.comm.get_linked_info()
        self.assert_equal(info["ssid"], "ESP32_Test_Lab")
        self.assert_equal(info["ip"], "192.168.1.108")
        self.assert_less(info["rssi"], 0, "RSSI should be negative dBm")

        # Disconnect
        ret_disc = self.comm.disconnect()
        self.assert_equal(ret_disc, WIFI_SUCCESS)
        self.assert_equal(self.comm.link_state, "DOWN")
        self.assert_equal(len(self.comm.get_linked_info()), 0)

    def test_wifi_scan_surrounding_aps(self) -> None:
        """Verify scanning surrounding access points and RSSI ordering."""
        self.comm.enable_wifi()
        self.assert_equal(self.comm.scan(), WIFI_SUCCESS)

        scan_list = self.comm.get_scan_info_list()
        self.assert_greater(len(scan_list), 0, "Should detect at least 1 AP")
        
        # Verify sorting: scan list should be sorted by RSSI descending
        rssis = [ap.rssi for ap in scan_list]
        self.assert_equal(rssis, sorted(rssis, reverse=True), "Scan list must be sorted by signal strength")

        # Check AP details
        first_ap = scan_list[0]
        self.assert_in("ESP32", first_ap.ssid)
        self.assert_in(first_ap.channel, range(1, 14))

    def test_wifi_softap_mode_and_stations(self) -> None:
        """Verify SoftAP hotspot mode configuration and connected station tracking."""
        self.comm.enable_wifi()
        ap_cfg = WifiDeviceConfig(ssid="ESP32_AP_HOTSPOT", psk="12345678", auth_type=WIFI_SEC_WPA2_PSK)
        self.comm.set_hotspot_config(ap_cfg)

        self.assert_equal(self.comm.enable_hotspot(), WIFI_SUCCESS)
        self.assert_true(self.comm.softap_enabled)

        # Station joins SoftAP
        self.comm.add_station_to_hotspot("AA:BB:CC:DD:EE:01", "192.168.4.2")
        sta_list = self.comm.get_station_list()
        self.assert_equal(len(sta_list), 1)
        self.assert_equal(sta_list[0]["mac"], "AA:BB:CC:DD:EE:01")

        self.assert_equal(self.comm.disable_hotspot(), WIFI_SUCCESS)
        self.assert_false(self.comm.softap_enabled)
        self.assert_equal(len(self.comm.get_station_list()), 0)

    def test_dhcp_client_ip_assignment(self) -> None:
        """Verify DHCP client lease and network gateway configuration."""
        self.comm.enable_wifi()
        cfg = WifiDeviceConfig(ssid="ESP32_Test_Lab")
        self.comm.connect_to(cfg)

        info = self.comm.get_linked_info()
        self.assert_in("ip", info)
        self.assert_in("netmask", info)
        self.assert_in("gateway", info)
        self.assert_in("dns", info)

        ip_parts = info["ip"].split(".")
        self.assert_equal(len(ip_parts), 4, "IPv4 address must have 4 octets")
        self.assert_equal(info["gateway"], "192.168.1.1")
        self.assert_equal(info["netmask"], "255.255.255.0")

    def test_tcp_socket_loopback(self) -> None:
        """Verify LwIP TCP socket stream communication over loopback."""
        conn, client = LwipSocketStack.create_tcp_server_client_pair()
        try:
            msg = b"PING_TCP_OHOS_ESP32"
            client.sendall(msg)

            received = conn.recv(len(msg))
            self.assert_equal(received, msg, "Server must receive exact TCP payload")

            # Echo response back
            conn.sendall(b"PONG_TCP_ACK")
            reply = client.recv(32)
            self.assert_equal(reply, b"PONG_TCP_ACK", "Client must receive exact TCP response")
        finally:
            conn.close()
            client.close()

    def test_tcp_socket_throughput_stream(self) -> None:
        """Verify large streaming transfer over TCP socket (32 KB payload)."""
        conn, client = LwipSocketStack.create_tcp_server_client_pair()
        try:
            payload_size = 32 * 1024
            chunk = b"X" * payload_size
            client.sendall(chunk)

            total_received = 0
            while total_received < payload_size:
                data = conn.recv(4096)
                if not data:
                    break
                total_received += len(data)

            self.assert_equal(total_received, payload_size, "All 32KB must be transferred without loss")
        finally:
            conn.close()
            client.close()

    def test_udp_socket_datagram_exchange(self) -> None:
        """Verify UDP socket datagram exchange."""
        s1, s2, port1 = LwipSocketStack.create_udp_pair()
        try:
            dgram = b"UDP_SENSOR_DATA_FRAME:99.8"
            s2.sendto(dgram, ("127.0.0.1", port1))

            data, addr = s1.recvfrom(1024)
            self.assert_equal(data, dgram, "UDP datagram payload must match exactly")
            self.assert_equal(addr[0], "127.0.0.1")
        finally:
            s1.close()
            s2.close()

    def test_socket_nonblocking_and_timeout(self) -> None:
        """Verify socket timeout handling."""
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(("127.0.0.1", 0))
        s.settimeout(0.01)  # 10ms timeout
        
        try:
            s.recvfrom(64)
            timed_out = False
        except socket.timeout:
            timed_out = True
        finally:
            s.close()

        self.assert_true(timed_out, "Socket recv on empty buffer must timeout")

    def test_netif_link_state_events(self) -> None:
        """Verify network interface state notification callbacks."""
        events = []
        def on_link_change(state: str) -> None:
            events.append(state)

        self.comm.add_link_listener(on_link_change)
        self.comm.enable_wifi()
        self.comm.connect_to(WifiDeviceConfig(ssid="EventAP"))
        self.comm.disconnect()

        self.assert_equal(events, ["UP", "DOWN"], "Link listener should receive UP and DOWN events sequentially")

"""
ACTS Mini System Compatibility Test Suite
Subsystem: SAMGR Lite (System Ability Manager Lite)
Target: ESP32 Architecture
"""

from xts_suite.core.xts_test_case import XtsTestCase
from xts_suite.subsystems.samgr_lite import (
    SamgrLiteManager, Service, Feature, IUnknown,
    EC_SUCCESS, EC_INVALID
)


class TestSamgrLite(XtsTestCase):
    """ACTS Compatibility Test Cases for OpenHarmony SAMGR Lite on ESP32."""

    subsystem = "SAMGR"
    module_name = "test_samgr_lite"

    def setUp(self) -> None:
        SamgrLiteManager.reset_instance()
        self.samgr = SamgrLiteManager.get_instance()

    def tearDown(self) -> None:
        SamgrLiteManager.reset_instance()
        self.samgr = None

    def test_samgr_instance_singleton(self) -> None:
        """Verify SAMGR Lite manager operates as a true singleton."""
        inst1 = SamgrLiteManager.get_instance()
        inst2 = SamgrLiteManager.get_instance()
        self.assert_equal(id(inst1), id(inst2), "SAMGR instances must reference identical memory singleton")

    def test_samgr_register_service(self) -> None:
        """Verify service registration, initialization lifecycle, and retrieval."""
        svc = Service("system_param_service")
        ret = self.samgr.register_service(svc)
        self.assert_equal(ret, EC_SUCCESS, "Registering valid service should return EC_SUCCESS")
        self.assert_true(svc.is_initialized, "Service on_initialize() must be invoked upon registration")

        retrieved = self.samgr.get_service("system_param_service")
        self.assert_is_not_none(retrieved)
        self.assert_equal(retrieved.name, "system_param_service")

    def test_samgr_duplicate_service_rejection(self) -> None:
        """Verify duplicate service name registration is rejected with EC_INVALID."""
        svc1 = Service("telephony_service")
        svc2 = Service("telephony_service")

        self.assert_equal(self.samgr.register_service(svc1), EC_SUCCESS)
        self.assert_equal(self.samgr.register_service(svc2), EC_INVALID, "Duplicate service registration must fail")

    def test_samgr_register_feature(self) -> None:
        """Verify feature registration under an existing registered service."""
        svc = Service("sensor_service")
        self.samgr.register_service(svc)

        feat = Feature("accelerometer_feature")
        ret = self.samgr.register_feature("sensor_service", feat)
        self.assert_equal(ret, EC_SUCCESS, "Registering feature under valid service should succeed")
        self.assert_in("accelerometer_feature", svc.features)
        self.assert_equal(feat.parent_service, "sensor_service")

    def test_samgr_feature_registration_invalid_service(self) -> None:
        """Verify registering feature under non-existent service returns EC_INVALID."""
        feat = Feature("gyro_feature")
        ret = self.samgr.register_feature("non_existent_service", feat)
        self.assert_equal(ret, EC_INVALID, "Registering feature to missing service must fail")

    def test_samgr_register_feature_api(self) -> None:
        """Verify feature IUnknown interface registration and query."""
        svc = Service("audio_service")
        self.samgr.register_service(svc)
        feat = Feature("volume_feature")
        self.samgr.register_feature("audio_service", feat)

        class AudioControlApi(IUnknown):
            def __init__(self):
                super().__init__("IID_AUDIO_CONTROL")
                self.volume = 50

            def set_volume(self, val: int) -> None:
                self.volume = val

        api = AudioControlApi()
        reg_api_ret = self.samgr.register_feature_api("audio_service", "volume_feature", api)
        self.assert_equal(reg_api_ret, EC_SUCCESS)

        # Retrieve API
        queried = self.samgr.get_feature_api("audio_service", "volume_feature", "IID_AUDIO_CONTROL")
        self.assert_is_not_none(queried)
        self.assert_equal(queried.interface_id, "IID_AUDIO_CONTROL")
        self.assert_equal(queried.volume, 50)
        queried.set_volume(85)
        self.assert_equal(queried.volume, 85)

    def test_samgr_message_routing(self) -> None:
        """Verify synchronous request-response routing through service."""
        svc = Service("kvstore_service")
        self.samgr.register_service(svc)

        req = {"msg_id": 101, "data": "GET_CONFIG"}
        resp = self.samgr.send_request("kvstore_service", req)
        self.assert_equal(resp["status"], EC_SUCCESS)
        self.assert_equal(resp["msg_id"], 101)
        self.assert_in("Handled by kvstore_service", resp["reply"])

        # Target missing service
        invalid_resp = self.samgr.send_request("ghost_service", req)
        self.assert_equal(invalid_resp["status"], EC_INVALID)

    def test_samgr_async_message_dispatch(self) -> None:
        """Verify message tracking and queue recording."""
        svc = Service("dispatcher_service")
        self.samgr.register_service(svc)

        self.assert_equal(self.samgr.dispatched_count, 0)
        self.samgr.send_request("dispatcher_service", {"msg_id": 1, "data": "D1"})
        self.samgr.send_request("dispatcher_service", {"msg_id": 2, "data": "D2"})

        self.assert_equal(self.samgr.dispatched_count, 2)
        self.assert_equal(len(self.samgr.message_queue), 2)
        self.assert_equal(len(svc.received_messages), 2)

    def test_samgr_unregister_feature_and_service(self) -> None:
        """Verify clean unregistration of feature and service."""
        svc = Service("temp_service")
        self.samgr.register_service(svc)
        feat = Feature("temp_feature")
        self.samgr.register_feature("temp_service", feat)

        # Unregister feature
        unreg_feat = self.samgr.unregister_feature("temp_service", "temp_feature")
        self.assert_equal(unreg_feat, EC_SUCCESS)
        self.assert_not_in("temp_feature", svc.features)

        # Unregister service
        unreg_svc = self.samgr.unregister_service("temp_service")
        self.assert_equal(unreg_svc, EC_SUCCESS)
        self.assert_false(svc.is_initialized, "Service on_stop() should have reset initialization")
        self.assert_is_none(self.samgr.get_service("temp_service"))

    def test_samgr_broadcast_pub_sub(self) -> None:
        """Verify broadcast publisher and multiple subscriber notification."""
        events_received_a = []
        events_received_b = []

        def sub_a(topic: str, data: any) -> None:
            events_received_a.append((topic, data))

        def sub_b(topic: str, data: any) -> None:
            events_received_b.append((topic, data))

        self.samgr.subscribe_broadcast("WIFI_STATE_CHANGED", sub_a)
        self.samgr.subscribe_broadcast("WIFI_STATE_CHANGED", sub_b)

        # Publish event
        pub_ret = self.samgr.publish_broadcast("WIFI_STATE_CHANGED", {"state": "CONNECTED", "ip": "192.168.1.100"})
        self.assert_equal(pub_ret, EC_SUCCESS)

        self.assert_equal(len(events_received_a), 1)
        self.assert_equal(len(events_received_b), 1)
        self.assert_equal(events_received_a[0][1]["state"], "CONNECTED")
        self.assert_equal(events_received_b[0][1]["ip"], "192.168.1.100")

"""
OpenHarmony SAMGR Lite (System Ability Manager Lite) for ESP32 Architecture
Subsystem: System Service Framework (Service, Feature, FeatureApi, Message Routing, Broadcast)
"""

import threading
from typing import Any, Callable, Optional

# Return codes
EC_SUCCESS = 0
EC_INVALID = -1
EC_NOPERM = -2
EC_NOMEMORY = -3
EC_ROUTER_FULL = -4


class IUnknown:
    """IUnknown interface for SAMGR Feature API."""
    def __init__(self, interface_id: str = "DEFAULT_IUNKNOWN"):
        self.interface_id = interface_id

    def query_interface(self, iid: str) -> Optional[Any]:
        if iid == self.interface_id:
            return self
        return None


class Feature:
    """SAMGR Lite Feature."""
    def __init__(self, name: str):
        self.name = name
        self.apis: dict[str, IUnknown] = {}
        self.parent_service: Optional[str] = None

    def on_initialize(self) -> None:
        pass

    def on_stop(self) -> None:
        pass


class Service:
    """SAMGR Lite Service."""
    def __init__(self, name: str):
        self.name = name
        self.features: dict[str, Feature] = {}
        self.default_api: Optional[IUnknown] = None
        self.is_initialized = False
        self.received_messages: list[dict[str, Any]] = []

    def on_initialize(self) -> None:
        self.is_initialized = True

    def on_stop(self) -> None:
        self.is_initialized = False

    def message_handle(self, request: dict[str, Any]) -> dict[str, Any]:
        self.received_messages.append(request)
        msg_id = request.get("msg_id", 0)
        data = request.get("data", None)
        return {"status": EC_SUCCESS, "msg_id": msg_id, "reply": f"Handled by {self.name}: {data}"}


class SamgrLiteManager:
    """SAMGR Lite Singleton Manager."""
    _instance: Optional["SamgrLiteManager"] = None
    _singleton_lock = threading.Lock()

    def __init__(self):
        self._lock = threading.RLock()
        self.services: dict[str, Service] = {}
        self.broadcast_subscribers: dict[str, list[Callable[[str, Any], None]]] = {}
        self.message_queue: list[dict[str, Any]] = []
        self.dispatched_count = 0

    @classmethod
    def get_instance(cls) -> "SamgrLiteManager":
        with cls._singleton_lock:
            if cls._instance is None:
                cls._instance = cls()
            return cls._instance

    @classmethod
    def reset_instance(cls) -> None:
        with cls._singleton_lock:
            cls._instance = None

    def register_service(self, service: Service) -> int:
        with self._lock:
            if not service or not service.name:
                return EC_INVALID
            if service.name in self.services:
                return EC_INVALID  # Duplicate service name rejected
            self.services[service.name] = service
            service.on_initialize()
            return EC_SUCCESS

    def unregister_service(self, name: str) -> int:
        with self._lock:
            if name not in self.services:
                return EC_INVALID
            svc = self.services.pop(name)
            svc.on_stop()
            return EC_SUCCESS

    def get_service(self, name: str) -> Optional[Service]:
        with self._lock:
            return self.services.get(name)

    def register_feature(self, service_name: str, feature: Feature) -> int:
        with self._lock:
            if service_name not in self.services or not feature or not feature.name:
                return EC_INVALID
            svc = self.services[service_name]
            if feature.name in svc.features:
                return EC_INVALID  # Duplicate feature
            feature.parent_service = service_name
            svc.features[feature.name] = feature
            feature.on_initialize()
            return EC_SUCCESS

    def unregister_feature(self, service_name: str, feature_name: str) -> int:
        with self._lock:
            if service_name not in self.services:
                return EC_INVALID
            svc = self.services[service_name]
            if feature_name not in svc.features:
                return EC_INVALID
            feat = svc.features.pop(feature_name)
            feat.on_stop()
            return EC_SUCCESS

    def register_feature_api(self, service_name: str, feature_name: str, api: IUnknown) -> int:
        with self._lock:
            if service_name not in self.services:
                return EC_INVALID
            svc = self.services[service_name]
            if feature_name not in svc.features:
                return EC_INVALID
            feat = svc.features[feature_name]
            feat.apis[api.interface_id] = api
            return EC_SUCCESS

    def get_feature_api(self, service_name: str, feature_name: str, interface_id: str = "DEFAULT_IUNKNOWN") -> Optional[IUnknown]:
        with self._lock:
            if service_name not in self.services:
                return None
            svc = self.services[service_name]
            if feature_name not in svc.features:
                return None
            feat = svc.features[feature_name]
            return feat.apis.get(interface_id)

    def send_request(self, target_service: str, request: dict[str, Any]) -> dict[str, Any]:
        with self._lock:
            if target_service not in self.services:
                return {"status": EC_INVALID, "error": "Target service not found"}
            svc = self.services[target_service]
            self.message_queue.append({"target": target_service, "req": request})
            self.dispatched_count += 1
            return svc.message_handle(request)

    def subscribe_broadcast(self, topic: str, handler: Callable[[str, Any], None]) -> int:
        with self._lock:
            if not topic or not callable(handler):
                return EC_INVALID
            if topic not in self.broadcast_subscribers:
                self.broadcast_subscribers[topic] = []
            self.broadcast_subscribers[topic].append(handler)
            return EC_SUCCESS

    def publish_broadcast(self, topic: str, data: Any) -> int:
        with self._lock:
            if topic not in self.broadcast_subscribers:
                return EC_SUCCESS
            for handler in self.broadcast_subscribers[topic]:
                try:
                    handler(topic, data)
                except Exception:
                    pass
            return EC_SUCCESS

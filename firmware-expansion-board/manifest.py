# Start with the complete upstream ESP32 module set, then freeze the project
# hardware helpers as an additional package.
include("$(PORT_DIR)/boards/manifest.py")
include("$(MPY_DIR)/extmod/txd/manifest.py")

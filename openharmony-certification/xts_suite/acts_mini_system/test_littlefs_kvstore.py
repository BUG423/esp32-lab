"""
ACTS Mini System Compatibility Test Suite
Subsystem: LittleFS & KVStore Persistence
Target: ESP32 Architecture
"""

from xts_suite.core.xts_test_case import XtsTestCase
from xts_suite.subsystems.littlefs_kvstore import (
    LittleFSEmulator, KVStore,
    LFS_ERR_OK, LFS_ERR_NOENT, LFS_ERR_EXIST,
    LFS_O_RDONLY, LFS_O_WRONLY, LFS_O_RDWR, LFS_O_CREAT, LFS_O_TRUNC, LFS_O_APPEND,
    LFS_SEEK_SET, LFS_SEEK_CUR, LFS_SEEK_END,
    LFS_TYPE_REG, LFS_TYPE_DIR,
    KEY_MAX_LEN, VALUE_MAX_LEN
)


class TestLittleFSKVStore(XtsTestCase):
    """ACTS Compatibility Test Cases for LittleFS & KVStore Subsystem on ESP32."""

    subsystem = "Storage"
    module_name = "test_littlefs_kvstore"

    def setUp(self) -> None:
        self.lfs = LittleFSEmulator(total_size=1024 * 1024)
        self.lfs.format()
        self.lfs.mount()
        self.kv = KVStore(self.lfs)

    def tearDown(self) -> None:
        if self.lfs:
            self.lfs.unmount()
        self.lfs = None
        self.kv = None

    def test_littlefs_format_and_mount(self) -> None:
        """Verify format, mount, and unmount state transitions."""
        self.assert_true(self.lfs.is_mounted)
        unmount_ret = self.lfs.unmount()
        self.assert_equal(unmount_ret, LFS_ERR_OK)
        self.assert_false(self.lfs.is_mounted)

        mount_ret = self.lfs.mount()
        self.assert_equal(mount_ret, LFS_ERR_OK)
        self.assert_true(self.lfs.is_mounted)

    def test_littlefs_file_create_write_read(self) -> None:
        """Verify file creation, payload write, and readback integrity."""
        path = "/test_file.bin"
        payload = b"OpenHarmony ESP32 Flash Payload Data"

        err, handle = self.lfs.file_open(path, LFS_O_CREAT | LFS_O_RDWR)
        self.assert_equal(err, LFS_ERR_OK)
        self.assert_is_not_none(handle)

        w_err, bytes_written = self.lfs.file_write(handle, payload)
        self.assert_equal(w_err, LFS_ERR_OK)
        self.assert_equal(bytes_written, len(payload))
        self.lfs.file_close(handle)

        # Reopen for read
        r_err, r_handle = self.lfs.file_open(path, LFS_O_RDONLY)
        self.assert_equal(r_err, LFS_ERR_OK)
        read_err, data = self.lfs.file_read(r_handle, len(payload))
        self.assert_equal(read_err, LFS_ERR_OK)
        self.assert_equal(data, payload, "Read data must exactly match written payload")
        self.lfs.file_close(r_handle)

    def test_littlefs_file_seek_modes(self) -> None:
        """Verify SEEK_SET, SEEK_CUR, and SEEK_END positioning."""
        path = "/seek_test.txt"
        _, h = self.lfs.file_open(path, LFS_O_CREAT | LFS_O_RDWR)
        self.lfs.file_write(h, b"0123456789ABCDEF")

        # SEEK_SET to offset 4
        _, pos = self.lfs.file_seek(h, 4, LFS_SEEK_SET)
        self.assert_equal(pos, 4)
        _, chunk = self.lfs.file_read(h, 2)
        self.assert_equal(chunk, b"45")

        # SEEK_CUR +3 (current pos was 6 -> becomes 9)
        _, pos2 = self.lfs.file_seek(h, 3, LFS_SEEK_CUR)
        self.assert_equal(pos2, 9)
        _, chunk2 = self.lfs.file_read(h, 2)
        self.assert_equal(chunk2, b"9A")

        # SEEK_END -2 (len=16 -> becomes 14)
        _, pos3 = self.lfs.file_seek(h, -2, LFS_SEEK_END)
        self.assert_equal(pos3, 14)
        _, chunk3 = self.lfs.file_read(h, 2)
        self.assert_equal(chunk3, b"EF")
        self.lfs.file_close(h)

    def test_littlefs_file_truncate_and_stat(self) -> None:
        """Verify file truncation and file metadata stat reporting."""
        path = "/trunc_test.dat"
        _, h = self.lfs.file_open(path, LFS_O_CREAT | LFS_O_RDWR)
        self.lfs.file_write(h, b"A" * 100)

        # Stat before truncation
        err_stat, meta = self.lfs.stat(path)
        self.assert_equal(err_stat, LFS_ERR_OK)
        self.assert_equal(meta["type"], LFS_TYPE_REG)
        self.assert_equal(meta["size"], 100)

        # Truncate to 30 bytes
        trunc_ret = self.lfs.file_truncate(h, 30)
        self.assert_equal(trunc_ret, LFS_ERR_OK)
        self.lfs.file_close(h)

        # Stat after truncation
        _, meta2 = self.lfs.stat(path)
        self.assert_equal(meta2["size"], 30, "File size must be 30 bytes after truncation")

    def test_littlefs_dir_and_unlink(self) -> None:
        """Verify directory creation, nesting, file removal (unlink), and dir deletion."""
        self.assert_equal(self.lfs.mkdir("/app_data"), LFS_ERR_OK)
        err_stat, meta = self.lfs.stat("/app_data")
        self.assert_equal(err_stat, LFS_ERR_OK)
        self.assert_equal(meta["type"], LFS_TYPE_DIR)

        # Create file inside directory
        fpath = "/app_data/config.json"
        _, h = self.lfs.file_open(fpath, LFS_O_CREAT | LFS_O_WRONLY)
        self.lfs.file_write(h, b'{"enabled": true}')
        self.lfs.file_close(h)

        # Remove file
        self.assert_equal(self.lfs.remove(fpath), LFS_ERR_OK)
        self.assert_equal(self.lfs.remove(fpath), LFS_ERR_NOENT, "Removing already deleted file must return LFS_ERR_NOENT")

        # Now remove empty directory
        self.assert_equal(self.lfs.remove("/app_data"), LFS_ERR_OK)

    def test_littlefs_power_loss_atomicity(self) -> None:
        """Verify power-loss resilience simulation preserving previous atomic state."""
        path = "/critical_data.bin"
        _, h = self.lfs.file_open(path, LFS_O_CREAT | LFS_O_RDWR)
        self.lfs.file_write(h, b"INITIAL_VALID_STATE")
        self.lfs.file_close(h)

        # Open again and begin writing new corrupt/interrupted data
        _, h2 = self.lfs.file_open(path, LFS_O_RDWR)
        self.lfs.file_write(h2, b"HALF_WRITTEN_DIRTY")
        
        # Simulate abrupt power-loss rollback
        self.lfs.simulate_power_loss_rollback(path)
        self.lfs.file_close(h2)

        # Verify rollback preserved original state
        _, h3 = self.lfs.file_open(path, LFS_O_RDONLY)
        _, recovered_data = self.lfs.file_read(h3, 64)
        self.lfs.file_close(h3)
        self.assert_equal(recovered_data, b"INITIAL_VALID_STATE", "Power loss rollback should restore previous atomic state")

    def test_kvstore_set_and_get_value(self) -> None:
        """Verify basic Key-Value pair storage and immediate retrieval."""
        key = "device_nickname"
        val = "ESP32-Node-01"

        set_ret = self.kv.set_value(key, val)
        self.assert_equal(set_ret, 0, "set_value should return 0 (Success)")

        retrieved = self.kv.get_value(key)
        self.assert_equal(retrieved, val, "Retrieved value must match stored value")

    def test_kvstore_delete_value(self) -> None:
        """Verify Key-Value deletion and verify get returns None."""
        key = "auth_token"
        self.kv.set_value(key, "secret_token_12345")
        self.assert_equal(self.kv.get_value(key), "secret_token_12345")

        del_ret = self.kv.delete_value(key)
        self.assert_equal(del_ret, 0, "delete_value should return 0")

        self.assert_is_none(self.kv.get_value(key), "Query on deleted key must return None")

    def test_kvstore_cache_consistency(self) -> None:
        """Verify cache eviction ensures direct LittleFS flash persistence and readback."""
        key = "persistent_wifi_ssid"
        val = "SmartHome-2.4G"
        self.kv.set_value(key, val)

        # Clear in-memory RAM cache
        self.kv.clear_cache()
        self.assert_not_in(key, self.kv.cache, "Key must not be in RAM cache after clear")

        # Readback from flash
        from_flash = self.kv.get_value(key)
        self.assert_equal(from_flash, val, "Value must be faithfully retrieved from LittleFS flash partition")
        self.assert_in(key, self.kv.cache, "Cache should be repopulated after read")

    def test_kvstore_boundary_and_validation(self) -> None:
        """Verify key length (<= 32) and value length (<= 1024) boundary limits."""
        # Key exactly 32 bytes
        valid_key = "K" * KEY_MAX_LEN
        valid_val = "V" * VALUE_MAX_LEN
        self.assert_equal(self.kv.set_value(valid_key, valid_val), 0)
        self.assert_equal(self.kv.get_value(valid_key), valid_val)

        # Invalid key length (33 bytes)
        invalid_key = "K" * (KEY_MAX_LEN + 1)
        self.assert_equal(self.kv.set_value(invalid_key, "data"), -1, "Key length > 32 must fail")

        # Invalid value length (1025 bytes)
        invalid_val = "V" * (VALUE_MAX_LEN + 1)
        self.assert_equal(self.kv.set_value("short_key", invalid_val), -1, "Value length > 1024 must fail")

"""
OpenHarmony LittleFS & KVStore Subsystem for ESP32 Architecture
Subsystem: Storage & Persistence (LittleFS block device, file ops, KVStore caching)
"""

import threading
import time
from typing import Any, Optional

# Return codes
LFS_ERR_OK = 0
LFS_ERR_IO = -5
LFS_ERR_CORRUPT = -84
LFS_ERR_NOENT = -2
LFS_ERR_EXIST = -17
LFS_ERR_NOTDIR = -20
LFS_ERR_ISDIR = -21
LFS_ERR_NOTEMPTY = -39
LFS_ERR_BADF = -9
LFS_ERR_INVAL = -22
LFS_ERR_NOSPC = -28

# File open flags
LFS_O_RDONLY = 1
LFS_O_WRONLY = 2
LFS_O_RDWR = 3
LFS_O_CREAT = 0x0100
LFS_O_EXCL = 0x0200
LFS_O_TRUNC = 0x0400
LFS_O_APPEND = 0x0800

# Seek whence
LFS_SEEK_SET = 0
LFS_SEEK_CUR = 1
LFS_SEEK_END = 2

# File Types
LFS_TYPE_REG = 1
LFS_TYPE_DIR = 2

# KVStore limits
KEY_MAX_LEN = 32
VALUE_MAX_LEN = 1024


class LittleFSFile:
    def __init__(self, path: str, flags: int):
        self.path = path
        self.flags = flags
        self.pos = 0
        self.closed = False


class LittleFSEmulator:
    """High-fidelity LittleFS File System Emulator for ESP32 Flash partition."""
    def __init__(self, total_size: int = 1024 * 1024, block_size: int = 4096):
        self.total_size = total_size
        self.block_size = block_size
        self._lock = threading.RLock()
        self.is_mounted = False
        self.files: dict[str, bytearray] = {}  # path -> bytearray
        self.directories: set[str] = {"/"}
        self.open_handles: dict[int, LittleFSFile] = {}
        self.next_handle = 1
        self.power_loss_journal: dict[str, bytearray] = {}

    def format(self) -> int:
        with self._lock:
            self.files.clear()
            self.directories = {"/"}
            self.open_handles.clear()
            self.is_mounted = False
            return LFS_ERR_OK

    def mount(self) -> int:
        with self._lock:
            self.is_mounted = True
            return LFS_ERR_OK

    def unmount(self) -> int:
        with self._lock:
            self.open_handles.clear()
            self.is_mounted = False
            return LFS_ERR_OK

    def _normalize_path(self, path: str) -> str:
        if not path.startswith("/"):
            path = "/" + path
        parts = [p for p in path.split("/") if p]
        return "/" + "/".join(parts)

    def mkdir(self, path: str) -> int:
        with self._lock:
            if not self.is_mounted:
                return LFS_ERR_IO
            norm = self._normalize_path(path)
            if norm in self.directories or norm in self.files:
                return LFS_ERR_EXIST
            self.directories.add(norm)
            return LFS_ERR_OK

    def file_open(self, path: str, flags: int) -> tuple[int, Optional[int]]:
        with self._lock:
            if not self.is_mounted:
                return LFS_ERR_IO, None
            norm = self._normalize_path(path)
            file_exists = norm in self.files

            if not file_exists:
                if not (flags & LFS_O_CREAT):
                    return LFS_ERR_NOENT, None
                self.files[norm] = bytearray()
            elif flags & LFS_O_EXCL and flags & LFS_O_CREAT:
                return LFS_ERR_EXIST, None

            # Snapshot existing file state upon opening for write/truncate
            if norm in self.files:
                self.power_loss_journal[norm] = bytearray(self.files[norm])

            if flags & LFS_O_TRUNC:
                self.files[norm] = bytearray()

            handle = self.next_handle
            self.next_handle += 1
            fobj = LittleFSFile(norm, flags)
            if flags & LFS_O_APPEND:
                fobj.pos = len(self.files[norm])
            self.open_handles[handle] = fobj
            return LFS_ERR_OK, handle

    def file_close(self, handle: int) -> int:
        with self._lock:
            if handle not in self.open_handles:
                return LFS_ERR_BADF
            fobj = self.open_handles.pop(handle)
            fobj.closed = True
            if fobj.path in self.files:
                self.power_loss_journal[fobj.path] = bytearray(self.files[fobj.path])
            return LFS_ERR_OK

    def file_write(self, handle: int, data: bytes) -> tuple[int, int]:
        with self._lock:
            if handle not in self.open_handles:
                return LFS_ERR_BADF, 0
            fobj = self.open_handles[handle]
            if (fobj.flags & 3) == LFS_O_RDONLY:
                return LFS_ERR_INVAL, 0
            
            buf = self.files[fobj.path]
            write_len = len(data)
            end_pos = fobj.pos + write_len
            if end_pos > len(buf):
                buf.extend(b'\x00' * (end_pos - len(buf)))
            buf[fobj.pos:end_pos] = data
            fobj.pos = end_pos
            return LFS_ERR_OK, write_len

    def file_read(self, handle: int, size: int) -> tuple[int, bytes]:
        with self._lock:
            if handle not in self.open_handles:
                return LFS_ERR_BADF, b""
            fobj = self.open_handles[handle]
            buf = self.files[fobj.path]
            if fobj.pos >= len(buf):
                return LFS_ERR_OK, b""
            chunk = bytes(buf[fobj.pos:fobj.pos + size])
            fobj.pos += len(chunk)
            return LFS_ERR_OK, chunk

    def file_seek(self, handle: int, offset: int, whence: int) -> tuple[int, int]:
        with self._lock:
            if handle not in self.open_handles:
                return LFS_ERR_BADF, -1
            fobj = self.open_handles[handle]
            buf = self.files[fobj.path]
            new_pos = fobj.pos
            if whence == LFS_SEEK_SET:
                new_pos = offset
            elif whence == LFS_SEEK_CUR:
                new_pos += offset
            elif whence == LFS_SEEK_END:
                new_pos = len(buf) + offset
            else:
                return LFS_ERR_INVAL, -1
            
            if new_pos < 0:
                return LFS_ERR_INVAL, -1
            fobj.pos = new_pos
            return LFS_ERR_OK, fobj.pos

    def file_truncate(self, handle: int, size: int) -> int:
        with self._lock:
            if handle not in self.open_handles:
                return LFS_ERR_BADF
            fobj = self.open_handles[handle]
            buf = self.files[fobj.path]
            if size < 0:
                return LFS_ERR_INVAL
            if size < len(buf):
                del buf[size:]
            elif size > len(buf):
                buf.extend(b'\x00' * (size - len(buf)))
            return LFS_ERR_OK

    def stat(self, path: str) -> tuple[int, Optional[dict[str, Any]]]:
        with self._lock:
            norm = self._normalize_path(path)
            if norm in self.directories:
                return LFS_ERR_OK, {"type": LFS_TYPE_DIR, "size": 0, "name": norm}
            if norm in self.files:
                return LFS_ERR_OK, {"type": LFS_TYPE_REG, "size": len(self.files[norm]), "name": norm}
            return LFS_ERR_NOENT, None

    def remove(self, path: str) -> int:
        with self._lock:
            norm = self._normalize_path(path)
            if norm in self.files:
                del self.files[norm]
                return LFS_ERR_OK
            if norm in self.directories:
                if norm == "/":
                    return LFS_ERR_INVAL
                # check if empty
                has_children = any(p.startswith(norm + "/") for p in self.files) or any(
                    d.startswith(norm + "/") for d in self.directories
                )
                if has_children:
                    return LFS_ERR_NOTEMPTY
                self.directories.remove(norm)
                return LFS_ERR_OK
            return LFS_ERR_NOENT

    def simulate_power_loss_rollback(self, path: str) -> None:
        """Simulate power-loss failure recovering previous atomic state."""
        with self._lock:
            norm = self._normalize_path(path)
            if norm in self.power_loss_journal:
                self.files[norm] = bytearray(self.power_loss_journal[norm])


class KVStore:
    """OpenHarmony KVStore (Key-Value Storage Service) for ESP32."""
    def __init__(self, lfs: LittleFSEmulator):
        self.lfs = lfs
        self._lock = threading.RLock()
        self.cache: dict[str, str] = {}
        self.kv_dir = "/kvstore"
        self._init_store()

    def _init_store(self) -> None:
        if not self.lfs.is_mounted:
            self.lfs.mount()
        self.lfs.mkdir(self.kv_dir)

    def set_value(self, key: str, value: str) -> int:
        with self._lock:
            if not key or len(key) > KEY_MAX_LEN or value is None or len(value) > VALUE_MAX_LEN:
                return -1
            # Update cache
            self.cache[key] = value
            # Write to LittleFS
            file_path = f"{self.kv_dir}/{key}.kv"
            err, h = self.lfs.file_open(file_path, LFS_O_CREAT | LFS_O_WRONLY | LFS_O_TRUNC)
            if err != LFS_ERR_OK or h is None:
                return -1
            self.lfs.file_write(h, value.encode("utf-8"))
            self.lfs.file_close(h)
            return 0

    def get_value(self, key: str) -> Optional[str]:
        with self._lock:
            if not key or len(key) > KEY_MAX_LEN:
                return None
            if key in self.cache:
                return self.cache[key]
            # Fallback to flash read
            file_path = f"{self.kv_dir}/{key}.kv"
            err, h = self.lfs.file_open(file_path, LFS_O_RDONLY)
            if err != LFS_ERR_OK or h is None:
                return None
            err, data = self.lfs.file_read(h, VALUE_MAX_LEN)
            self.lfs.file_close(h)
            if err == LFS_ERR_OK:
                val = data.decode("utf-8")
                self.cache[key] = val
                return val
            return None

    def delete_value(self, key: str) -> int:
        with self._lock:
            if not key or len(key) > KEY_MAX_LEN:
                return -1
            if key in self.cache:
                del self.cache[key]
            file_path = f"{self.kv_dir}/{key}.kv"
            err = self.lfs.remove(file_path)
            return 0 if err == LFS_ERR_OK else -1

    def clear_cache(self) -> None:
        """Evicts in-memory cache to verify flash persistence consistency."""
        with self._lock:
            self.cache.clear()

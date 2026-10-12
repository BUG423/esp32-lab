"""
OpenHarmony HUKS (Harmony Universal Key Store) Subsystem for ESP32 Architecture
Subsystem: Security & Cryptography (Key Generation, AES-CBC/GCM, SHA256, Secure Key Store)
"""

import hashlib
import hmac
import os
import threading
from typing import Any, Optional

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import padding

# HUKS Return Codes
HKS_SUCCESS = 0
HKS_FAILURE = -1
HKS_ERROR_INVALID_ARGUMENT = -2
HKS_ERROR_NOT_EXIST = -3
HKS_ERROR_ALREADY_EXISTS = -4
HKS_ERROR_MAC_INVALID = -5
HKS_ERROR_BUFFER_TOO_SMALL = -6

# Key Algorithms & Modes
HKS_ALG_AES = 1
HKS_ALG_HMAC = 2
HKS_ALG_SHA256 = 3

HKS_MODE_CBC = 1
HKS_MODE_GCM = 2
HKS_PADDING_NONE = 0
HKS_PADDING_PKCS7 = 1

HKS_KEY_SIZE_128 = 128
HKS_KEY_SIZE_256 = 256

HKS_PURPOSE_ENCRYPT = 1
HKS_PURPOSE_DECRYPT = 2
HKS_PURPOSE_MAC = 4
HKS_PURPOSE_DIGEST = 8


class HksBlob:
    """HUKS data blob."""
    def __init__(self, data: bytes = b""):
        self.data = data
        self.size = len(data)


class HksKeyRecord:
    """Internal key representation stored securely in keystore."""
    def __init__(self, alias: str, key_bytes: bytes, alg: int, key_size: int, purpose: int):
        self.alias = alias
        self.key_bytes = key_bytes
        self.alg = alg
        self.key_size = key_size
        self.purpose = purpose
        self.created_at = os.urandom(4).hex()


class HksCryptoHandle:
    """Session handle for 3-stage crypto flow (Init, Update, Finish)."""
    def __init__(self, handle_id: int, key_record: Optional[HksKeyRecord], purpose: int, mode: int, padding_mode: int, iv: bytes = b"", tag: bytes = b""):
        self.handle_id = handle_id
        self.key_record = key_record
        self.purpose = purpose
        self.mode = mode
        self.padding_mode = padding_mode
        self.iv = iv
        self.tag = tag
        self.buffer = bytearray()
        self.gcm_tag_result: bytes = b""


class HuksSecurityService:
    """OpenHarmony HUKS Core Engine."""
    def __init__(self):
        self._lock = threading.RLock()
        self.is_initialized = False
        self.key_store: dict[str, HksKeyRecord] = {}
        self.active_handles: dict[int, HksCryptoHandle] = {}
        self.next_handle_id = 1000

    def hks_initialize(self) -> int:
        with self._lock:
            self.is_initialized = True
            return HKS_SUCCESS

    def hks_refresh_key_info(self) -> int:
        with self._lock:
            if not self.is_initialized:
                return HKS_FAILURE
            return HKS_SUCCESS

    def hks_generate_key(self, alias: str, alg: int, key_size: int, purpose: int) -> int:
        with self._lock:
            if not self.is_initialized or not alias:
                return HKS_FAILURE
            if key_size not in (128, 256):
                return HKS_ERROR_INVALID_ARGUMENT
            num_bytes = key_size // 8
            key_bytes = os.urandom(num_bytes)
            self.key_store[alias] = HksKeyRecord(alias, key_bytes, alg, key_size, purpose)
            return HKS_SUCCESS

    def hks_key_exist(self, alias: str) -> int:
        with self._lock:
            if not self.is_initialized:
                return HKS_FAILURE
            return HKS_SUCCESS if alias in self.key_store else HKS_ERROR_NOT_EXIST

    def hks_delete_key(self, alias: str) -> int:
        with self._lock:
            if not self.is_initialized:
                return HKS_FAILURE
            if alias not in self.key_store:
                return HKS_ERROR_NOT_EXIST
            del self.key_store[alias]
            return HKS_SUCCESS

    def hks_get_key_count(self) -> int:
        with self._lock:
            return len(self.key_store)

    # ================= 3-Stage Crypto Flow =================
    def hks_init(self, alias: Optional[str], purpose: int, mode: int = HKS_MODE_CBC,
                 padding_mode: int = HKS_PADDING_PKCS7, iv: bytes = b"", tag: bytes = b"") -> tuple[int, Optional[int]]:
        with self._lock:
            if not self.is_initialized:
                return HKS_FAILURE, None
            key_rec = None
            if alias is not None:
                if alias not in self.key_store:
                    return HKS_ERROR_NOT_EXIST, None
                key_rec = self.key_store[alias]

            handle_id = self.next_handle_id
            self.next_handle_id += 1
            h = HksCryptoHandle(handle_id, key_rec, purpose, mode, padding_mode, iv, tag)
            self.active_handles[handle_id] = h
            return HKS_SUCCESS, handle_id

    def hks_update(self, handle_id: int, in_data: bytes) -> tuple[int, bytes]:
        with self._lock:
            if handle_id not in self.active_handles:
                return HKS_FAILURE, b""
            h = self.active_handles[handle_id]
            h.buffer.extend(in_data)
            return HKS_SUCCESS, b""

    def hks_finish(self, handle_id: int, in_data: bytes = b"") -> tuple[int, bytes, bytes]:
        """Returns (ret_code, output_data, auth_tag)."""
        with self._lock:
            if handle_id not in self.active_handles:
                return HKS_FAILURE, b"", b""
            h = self.active_handles.pop(handle_id)
            if in_data:
                h.buffer.extend(in_data)
            
            raw_input = bytes(h.buffer)
            
            # SHA-256 Digest
            if h.purpose == HKS_PURPOSE_DIGEST:
                digest = hashlib.sha256(raw_input).digest()
                return HKS_SUCCESS, digest, b""

            # HMAC
            if h.purpose == HKS_PURPOSE_MAC and h.key_record:
                mac = hmac.new(h.key_record.key_bytes, raw_input, hashlib.sha256).digest()
                return HKS_SUCCESS, mac, b""

            # AES
            if not h.key_record:
                return HKS_FAILURE, b"", b""

            key = h.key_record.key_bytes
            iv = h.iv or b"\x00" * 16

            if h.mode == HKS_MODE_CBC:
                if h.purpose == HKS_PURPOSE_ENCRYPT:
                    # Pad
                    if h.padding_mode == HKS_PADDING_PKCS7:
                        padder = padding.PKCS7(128).padder()
                        padded = padder.update(raw_input) + padder.finalize()
                    else:
                        padded = raw_input
                    cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
                    encryptor = cipher.encryptor()
                    out = encryptor.update(padded) + encryptor.finalize()
                    return HKS_SUCCESS, out, b""
                elif h.purpose == HKS_PURPOSE_DECRYPT:
                    cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
                    decryptor = cipher.decryptor()
                    decrypted = decryptor.update(raw_input) + decryptor.finalize()
                    if h.padding_mode == HKS_PADDING_PKCS7:
                        try:
                            unpadder = padding.PKCS7(128).unpadder()
                            out = unpadder.update(decrypted) + unpadder.finalize()
                        except Exception:
                            return HKS_FAILURE, b"", b""
                    else:
                        out = decrypted
                    return HKS_SUCCESS, out, b""

            elif h.mode == HKS_MODE_GCM:
                if h.purpose == HKS_PURPOSE_ENCRYPT:
                    cipher = Cipher(algorithms.AES(key), modes.GCM(iv))
                    encryptor = cipher.encryptor()
                    ciphertext = encryptor.update(raw_input) + encryptor.finalize()
                    tag = encryptor.tag
                    return HKS_SUCCESS, ciphertext, tag
                elif h.purpose == HKS_PURPOSE_DECRYPT:
                    try:
                        cipher = Cipher(algorithms.AES(key), modes.GCM(iv, h.tag))
                        decryptor = cipher.decryptor()
                        plaintext = decryptor.update(raw_input) + decryptor.finalize()
                        return HKS_SUCCESS, plaintext, b""
                    except Exception:
                        return HKS_ERROR_MAC_INVALID, b"", b""

            return HKS_FAILURE, b"", b""

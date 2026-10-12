"""
ACTS Mini System Compatibility Test Suite
Subsystem: HUKS (Harmony Universal Key Store) Security
Target: ESP32 Architecture
"""

import os
from xts_suite.core.xts_test_case import XtsTestCase
from xts_suite.subsystems.huks_security import (
    HuksSecurityService,
    HKS_SUCCESS, HKS_FAILURE, HKS_ERROR_NOT_EXIST, HKS_ERROR_MAC_INVALID,
    HKS_ALG_AES, HKS_ALG_HMAC, HKS_ALG_SHA256,
    HKS_MODE_CBC, HKS_MODE_GCM,
    HKS_PADDING_PKCS7, HKS_PADDING_NONE,
    HKS_KEY_SIZE_128, HKS_KEY_SIZE_256,
    HKS_PURPOSE_ENCRYPT, HKS_PURPOSE_DECRYPT, HKS_PURPOSE_MAC, HKS_PURPOSE_DIGEST
)


class TestHuksSecurity(XtsTestCase):
    """ACTS Compatibility Test Cases for HUKS Security Subsystem on ESP32."""

    subsystem = "HUKS"
    module_name = "test_huks_security"

    def setUp(self) -> None:
        self.huks = HuksSecurityService()
        self.huks.hks_initialize()

    def tearDown(self) -> None:
        self.huks = None

    def test_huks_init_and_refresh(self) -> None:
        """Verify HUKS subsystem initialization and key info refresh."""
        self.assert_true(self.huks.is_initialized)
        self.assert_equal(self.huks.hks_refresh_key_info(), HKS_SUCCESS)
        self.assert_equal(self.huks.hks_get_key_count(), 0)

    def test_huks_generate_aes_keys(self) -> None:
        """Verify generation of AES-128 and AES-256 cryptographic keys."""
        ret128 = self.huks.hks_generate_key("alias_aes_128", HKS_ALG_AES, HKS_KEY_SIZE_128, HKS_PURPOSE_ENCRYPT | HKS_PURPOSE_DECRYPT)
        self.assert_equal(ret128, HKS_SUCCESS)
        self.assert_equal(self.huks.hks_key_exist("alias_aes_128"), HKS_SUCCESS)
        self.assert_equal(len(self.huks.key_store["alias_aes_128"].key_bytes), 16)

        ret256 = self.huks.hks_generate_key("alias_aes_256", HKS_ALG_AES, HKS_KEY_SIZE_256, HKS_PURPOSE_ENCRYPT | HKS_PURPOSE_DECRYPT)
        self.assert_equal(ret256, HKS_SUCCESS)
        self.assert_equal(self.huks.hks_key_exist("alias_aes_256"), HKS_SUCCESS)
        self.assert_equal(len(self.huks.key_store["alias_aes_256"].key_bytes), 32)

    def test_huks_generate_hmac_keys(self) -> None:
        """Verify generation of HMAC keys and MAC operation."""
        alias = "alias_hmac_256"
        ret = self.huks.hks_generate_key(alias, HKS_ALG_HMAC, HKS_KEY_SIZE_256, HKS_PURPOSE_MAC)
        self.assert_equal(ret, HKS_SUCCESS)
        self.assert_equal(self.huks.hks_key_exist(alias), HKS_SUCCESS)

        # Compute HMAC
        _, handle = self.huks.hks_init(alias, HKS_PURPOSE_MAC)
        self.huks.hks_update(handle, b"DataToSign")
        ret_mac, mac_val, _ = self.huks.hks_finish(handle)
        self.assert_equal(ret_mac, HKS_SUCCESS)
        self.assert_equal(len(mac_val), 32, "HMAC-SHA256 output must be 32 bytes")

    def test_huks_key_storage_isolation(self) -> None:
        """Verify key alias isolation and storage uniqueness."""
        self.huks.hks_generate_key("k1", HKS_ALG_AES, 128, HKS_PURPOSE_ENCRYPT)
        self.huks.hks_generate_key("k2", HKS_ALG_AES, 128, HKS_PURPOSE_ENCRYPT)
        
        self.assert_equal(self.huks.hks_get_key_count(), 2)
        k1_bytes = self.huks.key_store["k1"].key_bytes
        k2_bytes = self.huks.key_store["k2"].key_bytes
        self.assert_not_equal(k1_bytes, k2_bytes, "Keys generated under different aliases must be distinct")

    def test_huks_delete_key(self) -> None:
        """Verify key deletion from secure storage."""
        alias = "transient_key"
        self.huks.hks_generate_key(alias, HKS_ALG_AES, 128, HKS_PURPOSE_ENCRYPT)
        self.assert_equal(self.huks.hks_key_exist(alias), HKS_SUCCESS)

        self.assert_equal(self.huks.hks_delete_key(alias), HKS_SUCCESS)
        self.assert_equal(self.huks.hks_key_exist(alias), HKS_ERROR_NOT_EXIST)
        self.assert_equal(self.huks.hks_delete_key(alias), HKS_ERROR_NOT_EXIST, "Deleting missing key must fail")

    def test_huks_aes_cbc_encrypt_decrypt(self) -> None:
        """Verify AES-128-CBC encryption and decryption with PKCS7 padding."""
        alias = "aes_cbc_key"
        self.huks.hks_generate_key(alias, HKS_ALG_AES, 128, HKS_PURPOSE_ENCRYPT | HKS_PURPOSE_DECRYPT)
        
        plaintext = b"OpenHarmony ESP32 AES-CBC Confidential Test Payload!"
        iv = os.urandom(16)

        # Encrypt
        _, enc_handle = self.huks.hks_init(alias, HKS_PURPOSE_ENCRYPT, mode=HKS_MODE_CBC, padding_mode=HKS_PADDING_PKCS7, iv=iv)
        self.huks.hks_update(enc_handle, plaintext)
        ret_enc, ciphertext, _ = self.huks.hks_finish(enc_handle)
        self.assert_equal(ret_enc, HKS_SUCCESS)
        self.assert_not_equal(ciphertext, plaintext)
        self.assert_equal(len(ciphertext) % 16, 0, "AES-CBC ciphertext length must be multiple of 16")

        # Decrypt
        _, dec_handle = self.huks.hks_init(alias, HKS_PURPOSE_DECRYPT, mode=HKS_MODE_CBC, padding_mode=HKS_PADDING_PKCS7, iv=iv)
        self.huks.hks_update(dec_handle, ciphertext)
        ret_dec, recovered, _ = self.huks.hks_finish(dec_handle)
        self.assert_equal(ret_dec, HKS_SUCCESS)
        self.assert_equal(recovered, plaintext, "Decrypted text must match original plaintext")

    def test_huks_aes_gcm_auth_encryption(self) -> None:
        """Verify AES-256-GCM authenticated encryption and valid tag verification."""
        alias = "aes_gcm_key"
        self.huks.hks_generate_key(alias, HKS_ALG_AES, 256, HKS_PURPOSE_ENCRYPT | HKS_PURPOSE_DECRYPT)

        plaintext = b"Secure Authenticated Firmware Packet #99"
        iv = os.urandom(12)  # 96-bit nonce for GCM

        # Encrypt
        _, enc_handle = self.huks.hks_init(alias, HKS_PURPOSE_ENCRYPT, mode=HKS_MODE_GCM, iv=iv)
        self.huks.hks_update(enc_handle, plaintext)
        ret_enc, ciphertext, tag = self.huks.hks_finish(enc_handle)
        self.assert_equal(ret_enc, HKS_SUCCESS)
        self.assert_equal(len(tag), 16, "GCM authentication tag must be 16 bytes")

        # Decrypt with correct tag
        _, dec_handle = self.huks.hks_init(alias, HKS_PURPOSE_DECRYPT, mode=HKS_MODE_GCM, iv=iv, tag=tag)
        self.huks.hks_update(dec_handle, ciphertext)
        ret_dec, decrypted, _ = self.huks.hks_finish(dec_handle)
        self.assert_equal(ret_dec, HKS_SUCCESS)
        self.assert_equal(decrypted, plaintext)

    def test_huks_aes_gcm_tamper_detection(self) -> None:
        """Verify AES-256-GCM detects any ciphertext or tag tampering."""
        alias = "aes_gcm_tamper_key"
        self.huks.hks_generate_key(alias, HKS_ALG_AES, 256, HKS_PURPOSE_ENCRYPT | HKS_PURPOSE_DECRYPT)

        plaintext = b"Critical Bank Transaction: Transfer $10000"
        iv = os.urandom(12)

        _, enc_handle = self.huks.hks_init(alias, HKS_PURPOSE_ENCRYPT, mode=HKS_MODE_GCM, iv=iv)
        self.huks.hks_update(enc_handle, plaintext)
        _, ciphertext, tag = self.huks.hks_finish(enc_handle)

        # Tamper 1 byte of ciphertext
        tampered_cipher = bytearray(ciphertext)
        tampered_cipher[0] ^= 0x01

        # Decrypt tampered ciphertext -> must fail authentication
        _, dec_handle = self.huks.hks_init(alias, HKS_PURPOSE_DECRYPT, mode=HKS_MODE_GCM, iv=iv, tag=tag)
        self.huks.hks_update(dec_handle, bytes(tampered_cipher))
        ret_dec, _, _ = self.huks.hks_finish(dec_handle)
        self.assert_equal(ret_dec, HKS_ERROR_MAC_INVALID, "Tampered ciphertext must fail tag verification")

    def test_huks_sha256_digest_nist_vectors(self) -> None:
        """Verify SHA-256 digest calculation matches NIST Cavp test vectors."""
        # NIST Test Vector 1: "abc"
        # SHA-256: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
        expected_hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        
        _, handle = self.huks.hks_init(None, HKS_PURPOSE_DIGEST)
        self.huks.hks_update(handle, b"abc")
        ret, digest, _ = self.huks.hks_finish(handle)

        self.assert_equal(ret, HKS_SUCCESS)
        self.assert_equal(digest.hex(), expected_hex, "SHA256 must match NIST test vector")

    def test_huks_three_stage_crypto_flow(self) -> None:
        """Verify multi-segment streaming update into finish."""
        _, handle = self.huks.hks_init(None, HKS_PURPOSE_DIGEST)
        chunks = [b"Chunk1_", b"Chunk2_", b"Chunk3_Final"]
        for c in chunks:
            self.assert_equal(self.huks.hks_update(handle, c)[0], HKS_SUCCESS)

        ret, digest, _ = self.huks.hks_finish(handle)
        self.assert_equal(ret, HKS_SUCCESS)
        self.assert_equal(len(digest), 32)

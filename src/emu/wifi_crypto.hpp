// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The cryptography of WPA2-PSK for SimNodus's virtual access point
// (experiment E-09, SN-267; our own code by decision DE9b): SHA-1 (FIPS 180-4),
// HMAC-SHA1 (RFC 2104), PBKDF2 (RFC 8018), the IEEE 802.11 PRF, AES-128
// (FIPS 197), AES key wrap (RFC 3394), CCM (RFC 3610) and CCMP (IEEE 802.11
// 12.5.3). Checked against the published test vectors in emu-esp32c3.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace simnodus::emu {

using Bytes = std::vector<std::uint8_t>;

std::array<std::uint8_t, 20> sha1(const Bytes& data);
std::array<std::uint8_t, 20> hmac_sha1(const Bytes& key, const Bytes& data);
// PBKDF2-HMAC-SHA1 with `iterations`, `length` bytes; WPA2's PMK is
// pbkdf2_sha1(passphrase, SSID, 4096, 32).
Bytes pbkdf2_sha1(const std::string& passphrase, const Bytes& salt, unsigned iterations, std::size_t length);
// IEEE 802.11 PRF-n: `bytes` of HMAC-SHA1(key, label || 0 || data || i).
Bytes prf_sha1(const Bytes& key, const std::string& label, const Bytes& data, std::size_t bytes);

// AES (FIPS 197) with a 16- or 32-byte key, one block at a time; also used
// by the ESP32-C3's AES accelerator.
class Aes {
public:
    explicit Aes(const std::uint8_t* key, std::size_t key_bytes = 16);
    std::array<std::uint8_t, 16> encrypt(const std::uint8_t* block) const;
    std::array<std::uint8_t, 16> decrypt(const std::uint8_t* block) const;

private:
    int rounds_ = 10;
    std::array<std::uint8_t, 240> round_keys_{};
};
using Aes128 = Aes;

// RFC 3394 key wrap of `plain` (a multiple of 8 bytes) with `kek`.
Bytes aes_key_wrap(const std::uint8_t* kek, const Bytes& plain);
// RFC 3610 CCM with an 8-byte MIC and a 13-byte nonce (L = 2): returns the
// ciphertext followed by the MIC; decrypt returns false if the MIC differs.
Bytes ccm_encrypt(const std::uint8_t* key, const std::uint8_t* nonce, const Bytes& aad, const Bytes& plain);
bool ccm_decrypt(const std::uint8_t* key, const std::uint8_t* nonce, const Bytes& aad, const Bytes& sealed, Bytes& plain);

// CCMP for an 802.11 data frame: `frame` is the plain MPDU (header and
// body, no FCS); the result has the 8-byte CCMP header after the MAC header
// and the 8-byte MIC at the end, and the Protected bit set. `pn` is the
// 48-bit packet number, `key_id` 0..3.
Bytes ccmp_encrypt(const std::uint8_t* tk, const Bytes& frame, std::uint64_t pn, int key_id);
// The reverse; false if the frame is too short or the MIC is wrong. `pn`
// receives the frame's packet number.
bool ccmp_decrypt(const std::uint8_t* tk, const Bytes& frame, Bytes& plain, std::uint64_t& pn);
// The length of an 802.11 data frame's MAC header (24, 26 with QoS, +6 with
// four addresses).
std::size_t wifi_header_length(const Bytes& frame);

} // namespace simnodus::emu

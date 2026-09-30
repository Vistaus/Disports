#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Discord's voice transport encryption (between the client and the voice
// server; DAVE adds end-to-end encryption on top): the RTP-size AEAD modes.
// The nonce is a 32-bit counter, big endian, at the start of an otherwise
// zero nonce, and sent after the ciphertext.
class TransportCipher
{
public:
    enum Mode { None, Aes256Gcm, XChaCha20Poly1305 };

    static constexpr size_t TagSize = 16;
    static constexpr size_t NonceSuffixSize = 4;

    // The best mode the voice server offers ("aead_aes256_gcm_rtpsize" is
    // preferred, "aead_xchacha20_poly1305_rtpsize" always available).
    static Mode pick(const std::vector<std::string>& offered);
    static const char* name(Mode mode);
    static Mode fromName(const std::string& mode);

    void setKey(Mode mode, const std::vector<uint8_t>& key);
    bool ready() const { return m_mode != None; }

    // ciphertext + tag
    bool seal(const uint8_t* aad, size_t aadSize, const uint8_t* plain, size_t plainSize,
              uint32_t nonce, std::vector<uint8_t>& out) const;
    // cipher is ciphertext + tag
    bool open(const uint8_t* aad, size_t aadSize, const uint8_t* cipher, size_t cipherSize,
              uint32_t nonce, std::vector<uint8_t>& out) const;

private:
    Mode m_mode = None;
    std::array<uint8_t, 32> m_key{};
};

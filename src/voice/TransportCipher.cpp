#include "TransportCipher.h"

#include <openssl/evp.h>

#include <algorithm>
#include <cstring>
#include <memory>

namespace {

struct CtxDeleter {
    void operator()(EVP_CIPHER_CTX* ctx) const { EVP_CIPHER_CTX_free(ctx); }
};
using CipherCtx = std::unique_ptr<EVP_CIPHER_CTX, CtxDeleter>;

void putNonce(uint8_t* out, uint32_t nonce)
{
    out[0] = uint8_t(nonce >> 24);
    out[1] = uint8_t(nonce >> 16);
    out[2] = uint8_t(nonce >> 8);
    out[3] = uint8_t(nonce);
}

// HChaCha20, to turn XChaCha20's 24 byte nonce into a ChaCha20 subkey
// (OpenSSL only has the IETF ChaCha20-Poly1305 with a 12 byte nonce).
uint32_t rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

void quarterRound(uint32_t* x, int a, int b, int c, int d)
{
    x[a] += x[b]; x[d] = rotl(x[d] ^ x[a], 16);
    x[c] += x[d]; x[b] = rotl(x[b] ^ x[c], 12);
    x[a] += x[b]; x[d] = rotl(x[d] ^ x[a], 8);
    x[c] += x[d]; x[b] = rotl(x[b] ^ x[c], 7);
}

uint32_t load32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
void store32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); p[2] = uint8_t(v >> 16); p[3] = uint8_t(v >> 24); }

void hchacha20(uint8_t out[32], const uint8_t key[32], const uint8_t nonce[16])
{
    static const uint8_t sigma[16] = {'e','x','p','a','n','d',' ','3','2','-','b','y','t','e',' ','k'};
    uint32_t x[16];
    for (int i = 0; i < 4; ++i) x[i] = load32(sigma + 4 * i);
    for (int i = 0; i < 8; ++i) x[4 + i] = load32(key + 4 * i);
    for (int i = 0; i < 4; ++i) x[12 + i] = load32(nonce + 4 * i);
    for (int i = 0; i < 10; ++i) {
        quarterRound(x, 0, 4, 8, 12); quarterRound(x, 1, 5, 9, 13);
        quarterRound(x, 2, 6, 10, 14); quarterRound(x, 3, 7, 11, 15);
        quarterRound(x, 0, 5, 10, 15); quarterRound(x, 1, 6, 11, 12);
        quarterRound(x, 2, 7, 8, 13); quarterRound(x, 3, 4, 9, 14);
    }
    for (int i = 0; i < 4; ++i) store32(out + 4 * i, x[i]);
    for (int i = 0; i < 4; ++i) store32(out + 16 + 4 * i, x[12 + i]);
}

// One AEAD operation with OpenSSL: AES-256-GCM or ChaCha20-Poly1305 (IETF).
bool aead(bool encrypt, const EVP_CIPHER* cipher, const uint8_t* key, const uint8_t* iv,
          const uint8_t* aad, size_t aadSize, const uint8_t* in, size_t inSize, std::vector<uint8_t>& out)
{
    const size_t tag = TransportCipher::TagSize;
    if (!encrypt && inSize < tag)
        return false;
    CipherCtx ctx(EVP_CIPHER_CTX_new());
    if (!ctx || !EVP_CipherInit_ex(ctx.get(), cipher, nullptr, nullptr, nullptr, encrypt ? 1 : 0)
            || !EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, 12, nullptr)
            || !EVP_CipherInit_ex(ctx.get(), nullptr, nullptr, key, iv, encrypt ? 1 : 0))
        return false;

    int len = 0;
    if (aadSize && !EVP_CipherUpdate(ctx.get(), nullptr, &len, aad, int(aadSize)))
        return false;

    const size_t bodySize = encrypt ? inSize : inSize - tag;
    out.resize(bodySize + (encrypt ? tag : 0));
    if (bodySize && !EVP_CipherUpdate(ctx.get(), out.data(), &len, in, int(bodySize)))
        return false;
    if (!encrypt && !EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG, int(tag),
                                         const_cast<uint8_t*>(in + bodySize)))
        return false;
    int finalLen = 0;
    if (!EVP_CipherFinal_ex(ctx.get(), out.data() + len, &finalLen))
        return false; // authentication failed
    if (encrypt && !EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG, int(tag), out.data() + bodySize))
        return false;
    return true;
}

}

TransportCipher::Mode TransportCipher::pick(const std::vector<std::string>& offered)
{
    auto has = [&](const char* mode) { return std::find(offered.begin(), offered.end(), mode) != offered.end(); };
    if (has("aead_aes256_gcm_rtpsize"))
        return Aes256Gcm;
    if (has("aead_xchacha20_poly1305_rtpsize"))
        return XChaCha20Poly1305;
    return None;
}

const char* TransportCipher::name(Mode mode)
{
    switch (mode) {
    case Aes256Gcm:         return "aead_aes256_gcm_rtpsize";
    case XChaCha20Poly1305: return "aead_xchacha20_poly1305_rtpsize";
    default:                return "";
    }
}

TransportCipher::Mode TransportCipher::fromName(const std::string& mode)
{
    if (mode == name(Aes256Gcm))
        return Aes256Gcm;
    if (mode == name(XChaCha20Poly1305))
        return XChaCha20Poly1305;
    return None;
}

void TransportCipher::setKey(Mode mode, const std::vector<uint8_t>& key)
{
    m_mode = key.size() == m_key.size() ? mode : None;
    if (m_mode != None)
        std::copy(key.begin(), key.end(), m_key.begin());
}

bool TransportCipher::seal(const uint8_t* aad, size_t aadSize, const uint8_t* plain, size_t plainSize,
                           uint32_t nonce, std::vector<uint8_t>& out) const
{
    uint8_t full[24] = {};
    putNonce(full, nonce);
    if (m_mode == Aes256Gcm)
        return aead(true, EVP_aes_256_gcm(), m_key.data(), full, aad, aadSize, plain, plainSize, out);
    if (m_mode == XChaCha20Poly1305) {
        uint8_t subkey[32];
        hchacha20(subkey, m_key.data(), full);
        uint8_t iv[12] = {};
        std::memcpy(iv + 4, full + 16, 8);
        return aead(true, EVP_chacha20_poly1305(), subkey, iv, aad, aadSize, plain, plainSize, out);
    }
    return false;
}

bool TransportCipher::open(const uint8_t* aad, size_t aadSize, const uint8_t* cipher, size_t cipherSize,
                           uint32_t nonce, std::vector<uint8_t>& out) const
{
    uint8_t full[24] = {};
    putNonce(full, nonce);
    if (m_mode == Aes256Gcm)
        return aead(false, EVP_aes_256_gcm(), m_key.data(), full, aad, aadSize, cipher, cipherSize, out);
    if (m_mode == XChaCha20Poly1305) {
        uint8_t subkey[32];
        hchacha20(subkey, m_key.data(), full);
        uint8_t iv[12] = {};
        std::memcpy(iv + 4, full + 16, 8);
        return aead(false, EVP_chacha20_poly1305(), subkey, iv, aad, aadSize, cipher, cipherSize, out);
    }
    return false;
}

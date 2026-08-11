#pragma once
#include <cstdint>
#include <cstddef>

// Lightweight obfuscation for the on-device flag reveal (not the
// authoritative check — that's flagdesk.cpp's SHA-256 compare). Just keeps
// the reveal string out of a plain `strings` dump.
// Each level uses its own distinct multi-byte XOR key (not a shared one)
// so brute-forcing one level's key can't decode every other level's flag.
namespace flagreveal {

// out must be at least len+1 bytes. keyLen must be > 0.
inline void decode(const uint8_t* obfuscated, size_t len, const uint8_t* key, size_t keyLen, char* out) {
    for (size_t i = 0; i < len; i++) out[i] = (char)(obfuscated[i] ^ key[i % keyLen]);
    out[len] = '\0';
}

} // namespace flagreveal

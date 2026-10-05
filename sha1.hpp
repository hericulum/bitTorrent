// sha1.hpp
#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

inline std::vector<uint8_t> computeSHA1(const std::vector<uint8_t>& data) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    uint64_t origBits = data.size() * 8;
    std::vector<uint8_t> padded = data;

    // 1. Append padding byte 0x80
    padded.push_back(0x80);

    // 2. Pad with 0x00 until length in bytes ≡ 56 (mod 64)
    while ((padded.size() % 64) != 56) {
        padded.push_back(0x00);
    }

    // 3. Append original length in bits as 64-bit big-endian integer
    for (int i = 7; i >= 0; --i) {
        padded.push_back(static_cast<uint8_t>((origBits >> (i * 8)) & 0xFF));
    }

    // 4. Process message in 512-bit (64-byte) chunks
    for (size_t chunk = 0; chunk < padded.size(); chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(padded[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(padded[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(padded[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(padded[chunk + i * 4 + 3]));
        }

        for (int i = 16; i < 80; ++i) {
            uint32_t val = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (val << 1) | (val >> 31); // Left rotate by 1
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    // 5. Construct 20-byte hash array
    std::vector<uint8_t> hash(20);
    auto writeUint32 = [](std::vector<uint8_t>& buf, size_t offset, uint32_t val) {
        buf[offset]     = (val >> 24) & 0xFF;
        buf[offset + 1] = (val >> 16) & 0xFF;
        buf[offset + 2] = (val >> 8) & 0xFF;
        buf[offset + 3] = val & 0xFF;
    };

    writeUint32(hash, 0, h0);
    writeUint32(hash, 4, h1);
    writeUint32(hash, 8, h2);
    writeUint32(hash, 12, h3);
    writeUint32(hash, 16, h4);

    return hash;
}
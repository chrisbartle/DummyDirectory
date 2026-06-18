#include "DDMD5Hasher.h"

#include <cstring>
#include <iomanip>
#include <sstream>

// Inline helpers stay fast here
inline uint32_t DDMD5Hasher::F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
inline uint32_t DDMD5Hasher::G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
inline uint32_t DDMD5Hasher::H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
inline uint32_t DDMD5Hasher::I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
inline uint32_t DDMD5Hasher::rotate_left(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

DDMD5Hasher::DDMD5Hasher() {
    reset();
}

/**
 * @brief DDMD5Hasher::reset
 * Call to reset the MD5 hash
 */
void DDMD5Hasher::reset() {
    count = 0;
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;
}

void DDMD5Hasher::transform(const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];

    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
        x[i] = ((uint32_t)block[j]) | (((uint32_t)block[j+1]) << 8) |
               (((uint32_t)block[j+2]) << 16) | (((uint32_t)block[j+3]) << 24);
    }

    static const uint32_t S[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    };
    static const uint32_t T[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };

    for (int i = 0; i < 64; ++i) {
        uint32_t f;
        int g;
        if (i < 16) { f = F(b, c, d); g = i; }
        else if (i < 32) { f = G(b, c, d); g = (5 * i + 1) % 16; }
        else if (i < 48) { f = H(b, c, d); g = (3 * i + 5) % 16; }
        else { f = I(b, c, d); g = (7 * i) % 16; }
        uint32_t temp = d;
        d = c; c = b;
        b = b + rotate_left(a + f + T[i] + x[g], S[i]);
        a = temp;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

/**
 * @brief DDMD5Hasher::update
 * Hand the hasher an arbitrary amount of data in order to increment the hash
 * @param input Data to be added to the hash
 * @param length Length of the data
 */
void DDMD5Hasher::update(const void* input, size_t length) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(input);
    size_t index = (size_t)((count >> 3) & 0x3F);
    count += (uint64_t)length << 3;

    size_t part_len = 64 - index;
    size_t i = 0;

    if (length >= part_len) {
        std::memcpy(&buffer[index], data, part_len);
        transform(buffer);
        for (i = part_len; i + 63 < length; i += 64) {
            transform(&data[i]);
        }
        index = 0;
    }
    std::memcpy(&buffer[index], &data[i], length - i);
}

/**
 * @brief DDMD5Hasher::finalize
 * Call this to retrieve the final MD5 hash. Once this is called then it is no longer
 * possible to update the hash.
 * @return Final md5 hash
 */
std::string DDMD5Hasher::finalize() {
    uint8_t bits[8];
    for (int i = 0; i < 8; ++i) {
        bits[i] = (uint8_t)((count >> (i * 8)) & 0xFF);
    }

    size_t index = (size_t)((count >> 3) & 0x3F);
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);

    static const uint8_t PADDING[64] = { 0x80 };
    update(PADDING, pad_len);
    update(bits, 8);

    std::ostringstream result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result << std::hex << std::setw(2) << std::setfill('0')
            << (int)((state[i] >> (j * 8)) & 0xFF);
        }
    }
    return result.str();
}
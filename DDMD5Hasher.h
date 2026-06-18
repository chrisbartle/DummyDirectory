#ifndef DDMD5HASHER_H
#define DDMD5HASHER_H

#include <string>
#include <cstdint>
#include <cstddef>

class DDMD5Hasher
{
public:
    DDMD5Hasher();
    void reset();
    void update(const void* input, size_t length);
    std::string finalize();

private:
    uint32_t state[4];   // A, B, C, D registers
    uint64_t count;      // Number of bits processed so far
    uint8_t buffer[64];  // Input buffer for incomplete blocks

    // Internal transformation helper functions
    static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z);
    static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z);
    static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z);
    static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z);
    static inline uint32_t rotate_left(uint32_t x, int n);

    void transform(const uint8_t block[64]);
};

#endif // DDMD5HASHER_H

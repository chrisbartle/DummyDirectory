#include "DeterministicPCGPRNG.h"

DeterministicPCGPRNG::DeterministicPCGPRNG() {}

/**
 * @brief DeterministicPCGPRNG::Seed
 * Give the DeterministicPCGPRNG a new random number seed and initialize the object to use it.
 * @param newSeed The 64 bit new seed value
 */
void DeterministicPCGPRNG::Seed(uint64_t newSeed)
{
    m_state = newSeed + m_increment;
    (void)get32();
}

/**
 * @brief DeterministicPCGPRNG::get32
 * Returns a pseudorandom 32 bit number
 * @return Unsigned 32 bit number
 */
uint32_t DeterministicPCGPRNG::get32()
{
    uint64_t oldstate = m_state;
    // Advance internal state (Standard LCG step)
    m_state = oldstate * 6364136223846793005ULL + m_increment;
    // Calculate permutation (XSH RR)
    uint32_t xorshifted = static_cast<uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = static_cast<uint32_t>(oldstate >> 59u);
    // Bitwise rotation
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

/**
 * @brief get64
 * Returns a pseudorandom 64 bit number
 * @return Unsigned 64 bit number
 */
uint64_t DeterministicPCGPRNG::get64()
{
    //This 64 bit value is just 2 32 bits stapled together
    uint64_t high = get32();
    uint64_t low = get32();
    return (high << 32) | low;
}

/**
 * @brief DeterministicPCGPRNG::getBytes
 * Return an arbitrary number of bytes
 * @param size The number of bytes to be returned
 * @return A vector containing the list of bytes
 */
std::vector<uint8_t> DeterministicPCGPRNG::getBytes(size_t size) {
    std::vector<uint8_t> buffer(size);
    size_t i = 0;

    // Fill 4 bytes at a time
    while (i + 4 <= size) {
        uint32_t randVal = get32();
        buffer[i]     = static_cast<uint8_t>(randVal & 0xFF);
        buffer[i + 1] = static_cast<uint8_t>((randVal >> 8) & 0xFF);
        buffer[i + 2] = static_cast<uint8_t>((randVal >> 16) & 0xFF);
        buffer[i + 3] = static_cast<uint8_t>((randVal >> 24) & 0xFF);
        i += 4;
    }

    // Handle remaining bytes if size is not a multiple of 4
    if (i < size) {
        uint32_t randVal = get32();
        while (i < size) {
            buffer[i] = static_cast<uint8_t>(randVal & 0xFF);
            randVal >>= 8;
            i++;
        }
    }
    return buffer;
}


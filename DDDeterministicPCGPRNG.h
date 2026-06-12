#ifndef DDDETERMINISTICPCGPRNG_H
#define DDDETERMINISTICPCGPRNG_H

#include <cstdint>
#include <vector>

/**
 * @brief The DeterministicPCGPRNG class
 * A deterministic random number generator; given the same seed value it will always
 * return the same results.
 * Uses the permuted congruential generator
 * https://en.wikipedia.org/wiki/Permuted_congruential_generator
 */
class DDDeterministicPCGPRNG
{
public:
    DDDeterministicPCGPRNG();

    void Seed(uint64_t newSeed);
    uint32_t get32();
    uint64_t get64();
    std::vector<uint8_t> getBytes(size_t size);

private:
    uint64_t       m_state      = 0;
    uint64_t const m_multiplier = 6364136223846793005u;
    uint64_t const m_increment  = 1442695040888963407u;	// Or an arbitrary odd constant
};

#endif // DDDETERMINISTICPCGPRNG_H

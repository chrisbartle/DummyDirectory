#ifndef DDDETERMINISTICPCGPRNG_H
#define DDDETERMINISTICPCGPRNG_H

#include <string>
#include <cstdint>
#include <vector>

using namespace std;

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
    DDDeterministicPCGPRNG(uint64_t newSeed = 0);

    void Seed(uint64_t newSeed);
    uint32_t get32();
    uint64_t get64();
    vector<uint8_t> getBytes(size_t size);
    string getSimpleString(size_t size);
    string getText(size_t size);
    uint64_t getFromRange(uint64_t min, uint64_t max);
    uint64_t processFlag(string inFlag, uint64_t inPercentageTotal = 0);
    static uint64_t convertStringToNumber(string inStr, uint64_t inPercentageTotal = 0);

private:
    uint64_t       m_state      = 0;
    uint64_t const m_multiplier = 6364136223846793005u;
    uint64_t const m_increment  = 1442695040888963407u;	// Or an arbitrary odd constant
    vector<string> m_textDictionary;
};

#endif // DDDETERMINISTICPCGPRNG_H

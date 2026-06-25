#include "DDDeterministicPCGPRNG.h"

#include <cmath>
#include <stdexcept>

DDDeterministicPCGPRNG::DDDeterministicPCGPRNG(uint64_t newSeed)
{
    if (newSeed > 0)
        Seed(newSeed);
    else
        m_state = m_increment;
}

/**
 * @brief DeterministicPCGPRNG::Seed
 * Give the DeterministicPCGPRNG a new random number seed and initialize the object to use it.
 * @param newSeed The 64 bit new seed value
 */
void DDDeterministicPCGPRNG::Seed(uint64_t newSeed)
{
    m_state = newSeed + m_increment;
    (void)get32();
}

/**
 * @brief DeterministicPCGPRNG::get32
 * Returns a pseudorandom 32 bit number
 * @return Unsigned 32 bit number
 */
uint32_t DDDeterministicPCGPRNG::get32()
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
uint64_t DDDeterministicPCGPRNG::get64()
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
std::vector<uint8_t> DDDeterministicPCGPRNG::getBytes(size_t size) {
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

/**
 * @brief DDDeterministicPCGPRNG::getSimpleString
 * Returns a string of only letters and numbers
 * @param size Number of characters in the string
 * @return String of characters
 */
std::string DDDeterministicPCGPRNG::getSimpleString(size_t size)
{
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    const uint64_t maxIndex = sizeof(charset) - 2; // -1 for null terminator, -1 for 0-based indexing

    std::string randomString;
    randomString.reserve(size); // Optimize memory allocation upfront

    // 2. Loop and pick a random character for each position
    for (size_t i = 0; i < size; ++i) {
        uint64_t randomIndex = getFromRange(0, maxIndex);
        randomString += charset[randomIndex];
    }

    return randomString;
}

/**
 * @brief DDDeterministicPCGPRNG::getFromRange
 * Pick a random number from a range (inclusive) of numbers
 * @param min Lowest possible number
 * @param max Highest possible number
 * @return Random number inside the range
 */
uint64_t DDDeterministicPCGPRNG::getFromRange(uint64_t min, uint64_t max)
{
    if (min > max) {
        throw std::invalid_argument("Minimum value cannot be greater than maximum value.");
    }

    // 1. Calculate the size of the range (inclusive)
    uint64_t range = max - min + 1;

    // Special case: If the range covers the entire 64-bit space,
    // we can just return a raw 64-bit random number.
    if (range == 0) {
        return get64();
    }

    // 2. Calculate the rejection threshold to eliminate modulo bias.
    // This finds the largest multiple of 'range' that fits in a 64-bit integer.
    uint64_t limit = -static_cast<uint64_t>(range) % range;

    // 3. Rejection sampling loop
    while (true) {
        uint64_t randVal = get64();
        if (randVal >= limit) {
            return min + (randVal % range);
        }
    }
}

/**
 * @brief DDDeterministicPCGPRNG::processFlag
 * This function processes the command line flags and converts it to a number. A flag will typically have one of
 * these forms:
 * - A number (12345)
 * - A range (100-200) in which case a random number is generated inside the range
 * - A percentage (55%) in which case the percentage of inPercentageTotal is returned
 * - A percentage range (10%-20%) in which case the percentages are converted into numbers and a number is chosen in the range
 * Note that numbers can be followed by K,M,G,T,P,E as multipliers (kilo,mega,giga,terra,peta,exa)
 * @param inFlag The text string of the flag as passed in by the user
 * @param inPercentageTotal A number indicating the value of 100% in this context. If this value is 0 then
 * percentage may not be used
 * @return An unsigned 64 bit number
 */
uint64_t DDDeterministicPCGPRNG::processFlag(string inFlag, uint64_t inPercentageTotal)
{
    try
    {
        uint64_t value;

        //Is this a range? If so, break it up.
        int dashPos = inFlag.find("-");
        if (dashPos != string::npos)
        {
            //It is a range
            string minStr = inFlag.substr(0, dashPos);
            string maxStr = inFlag.substr(dashPos+1);
            uint64_t min = convertStringToNumber(minStr, inPercentageTotal);
            uint64_t max = convertStringToNumber(maxStr, inPercentageTotal);
            value = getFromRange(min, max);
        }
        else
        {
            //It's not a range but a number.
            value = convertStringToNumber(inFlag, inPercentageTotal);
        }
        return value;
    }
    catch(...)
    {
        throw runtime_error("The following flag can not be parsed: " + inFlag);
    }
    return 0;
}

/**
 * @brief DDDeterministicPCGPRNG::convertStringToNumber
 * Given a number in a string format, convert to an unsigned 64 bit int. If this number ends with % then
 * the number is the percentage of inPercentageTotal.
 * This function will throw an exception if the number can't be converted
 * @param inStr The number in a string format. May end with %.
 * @param inPercentageTotal A number indicating the value of 100% in this context. If this value is 0 then
 * percentage may not be used
 * @return Unsigned 64 bit integer
 */
uint64_t DDDeterministicPCGPRNG::convertStringToNumber(string inStr, uint64_t inPercentageTotal)
{
    uint64_t val;
    uint64_t multiplier = 1;
    //Does this string end with a %?
    if (inStr.ends_with("%"))
    {
        if (inPercentageTotal == 0)
            throw runtime_error("Percentage not permitted");
        long double percent = std::stold(inStr.substr(0, inStr.length()-1)); //Remove the %
        if (percent < 0)
            throw runtime_error("Negative numbers are not allowed");
        long double factor = percent / 100.0l;
        long double calculated = static_cast<long double>(inPercentageTotal) * factor;
        if (calculated < 0.0l || calculated > static_cast<long double>(UINT64_MAX)) {
            throw runtime_error("Value is larger than 64 bit!");
        }
        val = static_cast<uint64_t>(round(calculated));
        return val;
    }

    //See if there are any multiplier characters on this string
    if (inStr.ends_with("k") || inStr.ends_with("K"))
        multiplier = 1024;
    else if (inStr.ends_with("m") || inStr.ends_with("M"))
        multiplier = 1048576;
    else if (inStr.ends_with("g") || inStr.ends_with("G"))
        multiplier = 1073741824;
    else if (inStr.ends_with("t") || inStr.ends_with("T"))
        multiplier = 1099511627776;
    else if (inStr.ends_with("p") || inStr.ends_with("P"))
        multiplier = 1125899906842620;
    else if (inStr.ends_with("e") || inStr.ends_with("E"))
        multiplier = 1152921504606850000;

    //The user may have entered something like 2.5G. Assume its a floating
    //point, apply the multiplier, and round it to an int.
    long double userNumber;
    if (multiplier > 1)
        userNumber = std::stold(inStr.substr(0, inStr.length()-1)); //Remove the suffix
    else
        userNumber = std::stold(inStr);
    long double calculated = static_cast<long double>(userNumber) * multiplier;
    val = static_cast<uint64_t>(round(calculated));
    return val;
}


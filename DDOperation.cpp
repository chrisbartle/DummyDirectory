#include "DDOperation.h"
#include "DDDeterministicPCGPRNG.h"

#include <random>

using namespace std;

DDOperation::DDOperation(DDManifest &inManifest) : m_manifest(inManifest)
{

}

/**
 * @brief DDOperation::DoOperation
 * Perform an operation in the dummy directory
 * @param parameters The parameters to apply against this operation
 */
void DDOperation::DoOperation(DDParameters &parameters)
{
    //Make sure that the operation is valid
    if (!ValidateOperationType(parameters.getOperation()))
        throw std::runtime_error("Operation type " + parameters.getOperation() + " is not known");

    //Initialize the deterministic random number generator
    DDDeterministicPCGPRNG rng;
    if (!parameters.isFlag("seed"))
    {
        //A predefined seed value is not being provided, we'll generate our own
        //and store it in the parameters so that it gets logged properly.
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 engine(rd());
        static std::uniform_int_distribution<uint64_t> dist; // Default range is min() to max()
        string newSeed = format("{:x}", dist(engine));
        parameters.setFlag("seed", newSeed);
    }
    //The parameter contains the seed value, use this as the seed
    try
    {
        rng.Seed(stoull(parameters.getFlag("seed"), nullptr, 16));
    }
    catch(...)
    {
        throw runtime_error("The following seed value can not be parsed: " + parameters.getFlag("seed"));
    }
}

/**
 * @brief DDOperation::ValidateOperationType
 * Given the name of an operation (add, delete, modify, etc), confirms that it is a valid
 * and recognizable type.
 * @param inOperation The type of operation to be run
 * @return true if the operation is a valid type
 */
bool DDOperation::ValidateOperationType(std::string inOperation)
{
    if (inOperation == "add")
        return true;
    return false;
}

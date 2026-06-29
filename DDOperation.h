#ifndef DDOPERATION_H
#define DDOPERATION_H

#include <atomic>
#include <memory>

#include "DDManifest.h"
#include "DDParameters.h"
#include "DDDeterministicPCGPRNG.h"
#include "BS_thread_pool.hpp"

class DDOperation
{
public:
    const int BUFFER_SIZE = 1000;

    DDOperation(DDManifest &inManifest);
    static std::unique_ptr<DDOperation> getOperation(std::string inOperation, DDManifest &inManifest);
    void SetDefaultParameters(DDParameters &parameters);
    void DoOperation(DDParameters &parameters);

    static std::string ValidateOperationType(std::string inOperation);
    static std::string ValidateFlag(std::string inOperation, std::string inFlag, std::string inFlagValue);

protected:
    void DoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size);

    virtual void ChildSetDefaultParameters(DDParameters &parameters) {};
    virtual void ChildDoOperation(DDParameters &parameters) {};
    virtual void ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size) {};

    DDManifest &m_manifest;
    std::string m_filePrefix;

    std::atomic_uint64_t m_processedSize;
    std::atomic_uint64_t m_processedCount;

    DDDeterministicPCGPRNG m_rng;
    std::unique_ptr<BS::light_thread_pool> m_threadPool = NULL;
};

#endif // DDOPERATION_H

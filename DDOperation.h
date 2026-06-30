#ifndef DDOPERATION_H
#define DDOPERATION_H

#include <atomic>
#include <memory>
#include <chrono>

#include "DDManifest.h"
#include "DDParameters.h"
#include "DDDeterministicPCGPRNG.h"
#include "BS_thread_pool.hpp"

class DDOperation
{
public:
    const int BUFFER_SIZE = 1000;

    DDOperation(DDManifest &inManifest);
    static std::unique_ptr<DDOperation> getOperationByName(std::string inOperation, DDManifest &inManifest);
    void SetDefaultParameters(DDParameters &parameters);
    void DoOperation(DDParameters &parameters);
    virtual std::string GetOperationSummation();
    void setStatusCallbackFunction(std::function<void(double)> inFunction) { m_statusCallbackFunction = inFunction; }

    static std::string ValidateOperationType(std::string inOperation);
    static std::string ValidateFlag(std::string inOperation, std::string inFlag, std::string inFlagValue);

protected:
    void DoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size);

    virtual void ChildSetDefaultParameters(DDParameters &parameters) {};
    virtual void ChildDoOperation(DDParameters &parameters) {};
    virtual void ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size) {};

    DDManifest &m_manifest;
    std::string m_filePrefix;

    std::function<void(double)> m_statusCallbackFunction;
    std::atomic_uint64_t m_processedSize;
    std::atomic_uint64_t m_processedCount;
    std::atomic_uint64_t m_targetSize;
    std::atomic_uint64_t m_targetCount;
    std::chrono::steady_clock::time_point m_startProcessing;
    std::chrono::steady_clock::time_point m_endProcessing;

    DDDeterministicPCGPRNG m_rng;
    std::unique_ptr<BS::light_thread_pool> m_threadPool = NULL;
};

#endif // DDOPERATION_H

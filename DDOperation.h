#ifndef DDOPERATION_H
#define DDOPERATION_H

#include <atomic>

#include "DDManifest.h"
#include "DDParameters.h"

class DDOperation
{
public:
    const int BUFFER_SIZE = 1000;

    DDOperation(DDManifest &inManifest);

    void SetDefaultParameters(DDParameters &parameters);
    void DoOperation(DDParameters &parameters);
    void DoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size);

    static std::string ValidateOperationType(std::string inOperation);
    static std::string ValidateFlag(std::string inOperation, std::string inFlag, std::string inFlagValue);

private:
    DDManifest &m_manifest;
    std::string m_filePrefix;

    std::atomic_uint64_t m_processedSize;
    std::atomic_uint64_t m_processedCount;
};

#endif // DDOPERATION_H

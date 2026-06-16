#ifndef DDOPERATION_H
#define DDOPERATION_H

#include "DDManifest.h"
#include "DDParameters.h"

class DDOperation
{
public:


    DDOperation(DDManifest &inManifest);

    void DoOperation(DDParameters &parameters);

    static std::string ValidateOperationType(std::string inOperation);
    static std::string ValidateFlag(std::string inOperation, std::string inFlag, std::string inFlagValue);

private:
    DDManifest &m_manifest;
};

#endif // DDOPERATION_H

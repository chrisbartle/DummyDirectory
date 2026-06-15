#ifndef DDOPERATION_H
#define DDOPERATION_H

#include "DDManifest.h"
#include "DDParameters.h"

class DDOperation
{
public:
    DDOperation(DDManifest &inManifest);

    void DoOperation(DDParameters &parameters);

    static bool ValidateOperationType(std::string inOperation);

private:
    DDManifest &m_manifest;
};

#endif // DDOPERATION_H

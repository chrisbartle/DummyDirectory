#ifndef DDOPERATIONMODIFY_H
#define DDOPERATIONMODIFY_H

#include "DDOperation.h"
#include "DDMD5Hasher.h"
#include "DDDeterministicPCGPRNG.h"

class DDOperationModify : public DDOperation
{
public:
    using DDOperation::DDOperation;

    enum ModificationType { APPEND, TRUNCATE, OVERWRITE, CHOP, INSERT, ModificationTypeCount };
    static ModificationType ConvertStringToModifcationType(string inModificationType);
protected:
    virtual void ChildSetDefaultParameters(DDParameters &parameters) override;
    virtual void ChildDoOperation(DDParameters &parameters) override;
    virtual void ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size) override;
    virtual std::string GetOperationSummation() override;

    void readFile(fstream &inFile, DDMD5Hasher &hasher, uint64_t size);
    void writeFile(fstream &inFile, string fileExtension, DDMD5Hasher &hasher, DDDeterministicPCGPRNG &inRNG, uint64_t size);

    uint64_t m_processedFileSizeTotal;
};

#endif // DDOPERATIONMODIFY_H

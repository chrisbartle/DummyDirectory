#ifndef DDOPERATIONRENAME_H
#define DDOPERATIONRENAME_H

#include "DDOperation.h"

class DDOperationRename : public DDOperation
{
public:
    using DDOperation::DDOperation;

protected:
    virtual void ChildSetDefaultParameters(DDParameters &parameters) override;
    virtual void ChildDoOperation(DDParameters &parameters) override;
    virtual void ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size) override;
    virtual std::string GetOperationSummation() override;
};

#endif // DDOPERATIONRENAME_H

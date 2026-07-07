#ifndef DDOPERATIONREBUILD_H
#define DDOPERATIONREBUILD_H

#include "DDOperation.h"

class DDOperationRebuild : public DDOperation
{
public:
    using DDOperation::DDOperation;

protected:
    virtual void ChildDoOperation(DDParameters &parameters) override;
    virtual void ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size) override;
    virtual std::string GetOperationSummation() override;
};

#endif // DDOPERATIONREBUILD_H

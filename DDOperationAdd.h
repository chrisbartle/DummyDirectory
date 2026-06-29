#ifndef DDOPERATIONADD_H
#define DDOPERATIONADD_H

#include "DDOperation.h"

class DDOperationAdd : public DDOperation
{
public:
    using DDOperation::DDOperation;

protected:
    virtual void ChildSetDefaultParameters(DDParameters &parameters) override;
    virtual void ChildDoOperation(DDParameters &parameters) override;
    virtual void ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size) override;
};

#endif // DDOPERATIONADD_H

#ifndef DDOPERATIONCLEAN_H
#define DDOPERATIONCLEAN_H

#include "DDOperation.h"

class DDOperationClean : public DDOperation
{
public:
    using DDOperation::DDOperation;

protected:
    virtual void ChildDoOperation(DDParameters &parameters) override;
    virtual void ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size) override;
    virtual std::string GetOperationSummation() override;
};

#endif // DDOPERATIONCLEAN_H

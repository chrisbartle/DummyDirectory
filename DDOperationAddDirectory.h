#ifndef DDOPERATIONADDDIRECTORY_H
#define DDOPERATIONADDDIRECTORY_H

#include "DDOperation.h"

class DDOperationAddDirectory : public DDOperation
{
public:
    using DDOperation::DDOperation;

protected:
    virtual void ChildSetDefaultParameters(DDParameters &parameters) override;
    virtual void ChildDoOperation(DDParameters &parameters) override;
};

#endif // DDOPERATIONADDDIRECTORY_H

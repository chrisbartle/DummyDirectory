#ifndef DDOPERATIONRENAMEDIRECTORY_H
#define DDOPERATIONRENAMEDIRECTORY_H

#include "DDOperation.h"
#include "DDDirectory.h"

#include <filesystem>

class DDOperationRenameDirectory : public DDOperation
{
public:
    using DDOperation::DDOperation;

protected:
    virtual void ChildSetDefaultParameters(DDParameters &parameters) override;
    virtual void ChildDoOperation(DDParameters &parameters) override;
    virtual std::string GetOperationSummation() override;

private:
    uint64_t RenameOneDirectory(DDDirectory &directory, DDParameters &parameters);
    uint64_t UpdateDescendantPaths(const std::filesystem::path &oldPath, const std::filesystem::path &newPath);

    static bool IsDescendantPath(const std::filesystem::path &path, const std::filesystem::path &ancestor);
    static std::filesystem::path ReplacePathPrefix(const std::filesystem::path &path, const std::filesystem::path &oldPrefix, const std::filesystem::path &newPrefix);
};

#endif // DDOPERATIONRENAMEDIRECTORY_H

#ifndef DDOPERATIONMOVEDIRECTORY_H
#define DDOPERATIONMOVEDIRECTORY_H

#include "DDOperation.h"
#include "DDDirectory.h"

#include <filesystem>
#include <vector>

class DDOperationMoveDirectory : public DDOperation
{
public:
    using DDOperation::DDOperation;

protected:
    virtual void ChildSetDefaultParameters(DDParameters &parameters) override;
    virtual void ChildDoOperation(DDParameters &parameters) override;
    virtual std::string GetOperationSummation() override;

private:
    uint64_t MoveOneDirectory(DDDirectory &directory, DDParameters &parameters, uint64_t maxDepth);
    uint64_t UpdateDescendantPaths(const std::filesystem::path &oldPath, const std::filesystem::path &newPath);
    uint64_t GetSubtreeMaxDepth(const std::filesystem::path &directoryPath, uint64_t directoryDepth);
    uint64_t GetSubtreeMaxDepthSorted(uint64_t directoryPos);
    std::vector<uint64_t> FindCandidateDestinations(const std::filesystem::path &directoryPath, uint64_t subtreeExtraDepth, uint64_t maxDepth);
    bool HasCandidateDestination(const std::filesystem::path &directoryPath, uint64_t subtreeExtraDepth, uint64_t maxDepth);
    bool IsValidDestination(DDDirectory &candidate, const std::filesystem::path &directoryPath, const std::filesystem::path &currentParentPath, uint64_t subtreeExtraDepth, uint64_t maxDepth);

    static bool IsDescendantPath(const std::filesystem::path &path, const std::filesystem::path &ancestor);
    static std::filesystem::path ReplacePathPrefix(const std::filesystem::path &path, const std::filesystem::path &oldPrefix, const std::filesystem::path &newPrefix);

    //The number of individual files whose path was affected by a directory move. This is
    //distinct from m_processedCount, which counts the directories themselves.
    uint64_t m_filesAffected = 0;
};

#endif // DDOPERATIONMOVEDIRECTORY_H

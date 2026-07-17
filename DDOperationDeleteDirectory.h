#ifndef DDOPERATIONDELETEDIRECTORY_H
#define DDOPERATIONDELETEDIRECTORY_H

#include "DDOperation.h"
#include "DDDirectory.h"

#include <filesystem>
#include <atomic>

class DDOperationDeleteDirectory : public DDOperation
{
public:
    using DDOperation::DDOperation;

protected:
    virtual void ChildSetDefaultParameters(DDParameters &parameters) override;
    virtual void ChildDoOperation(DDParameters &parameters) override;
    virtual void ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size) override;
    virtual std::string GetOperationSummation() override;

private:
    uint64_t QueueDirectoryForDeletion(DDDirectory &directory, DDParameters &parameters);
    void RemoveClaimedDirectories(DDParameters &parameters);

    static bool IsDescendantPath(const std::filesystem::path &path, const std::filesystem::path &ancestor);

    //The number of individual files actually removed by this operation. This is incremented
    //from worker threads inside ChildDoFileOperation, so it needs to be atomic - unlike
    //m_processedCount (inherited from DDOperation), which this class deliberately reserves
    //for counting directories rather than files.
    std::atomic_uint64_t m_filesAffected = 0;
};

#endif // DDOPERATIONDELETEDIRECTORY_H

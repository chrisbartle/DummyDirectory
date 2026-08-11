#include "DDOperationAddDirectory.h"


void DDOperationAddDirectory::ChildSetDefaultParameters(DDParameters &parameters)
{
    //With directory operations, we only care about the count
    if (!parameters.isFlag("count"))
    {
        //10 directories is a good default
        parameters.setFlag("count", "10");
    }
}

void DDOperationAddDirectory::ChildDoOperation(DDParameters &parameters)
{
    //We need to determine our target (number of directories to be added)
    m_targetCount = m_rng.processFlag(parameters.getFlag("count"), m_manifest.getTotalFileCount());

    //Determine the maximum depth of the directory structure
    uint64_t maxDepth = 2;
    if (parameters.isFlag("maxdepth"))
        maxDepth = stoul(parameters.getFlag("maxdepth"));

    if (maxDepth == 0)
        throw runtime_error("It is not possible to add directories if the maximum depth is set to 0");

    //Loop until the target count is reached
    while (m_processedCount < m_targetCount)
    {
        //Create the new directory entry but set its status to processing so that it doesn't get picked by the random
        //selector.
        DDDirectory &newDirectory = m_manifest.addDirectory();
        newDirectory.setProcessingStatus(DDDirectory::PROCESSING);
        //Pick a directory where this new directory will be added. It must be at least one lower than the max depth
        //so the new directory doesn't exceed it.
        DDDirectory &parentDirectory = m_manifest.getRandomDirectory(m_rng, maxDepth-1);
        string directoryName = "DD_" + m_rng.getSimpleString(10);
        newDirectory.setRelativePath(parentDirectory.relativePath() / directoryName);
        filesystem::path absolutePath = parameters.ConvertToAbsolutePath(newDirectory.relativePath());
        try
        {
            //We don't want to overwrite an existing file
            if (std::filesystem::exists(absolutePath))
            {
                newDirectory.setProcessingStatus(DDDirectory::CONFLICT);
                continue;
            }
            std::filesystem::create_directory(absolutePath);
        }
        catch (const std::exception& e) {
            newDirectory.recordProcessingError("Exception thrown when processing directory " + absolutePath.string() + ": " + e.what());
            return;
        }
        catch(...)
        {
            newDirectory.recordProcessingError("Unknown exception when processing directory " + absolutePath.string());
            return;
        }

        newDirectory.setProcessingStatus(DDDirectory::COMPLETE);
        //Increment
        m_processedCount++;
        //Update the status every 5 directories
        if (m_processedCount%5 == 0)
            UpdateProcessingStatus();
    }
}

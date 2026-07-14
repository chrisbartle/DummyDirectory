#include "DDOperationRenameDirectory.h"

#include <numeric>
#include <format>
#include <locale>

using namespace std;

void DDOperationRenameDirectory::ChildSetDefaultParameters(DDParameters &parameters)
{
    //Either the size or the count must be filled in. A directory's "size" is the sum of the
    //sizes of every file nested underneath it.
    if (!parameters.isFlag("size") && !parameters.isFlag("count"))
    {
        //Rename 10% as the default
        parameters.setFlag("count", "10%");
    }
}

void DDOperationRenameDirectory::ChildDoOperation(DDParameters &parameters)
{
    uint64_t totalDirectoryCount = m_manifest.getTotalDirectoryCount();
    //Position 0 is always the root directory and it can never be renamed, so there must be
    //at least one other directory for this operation to do anything.
    if (totalDirectoryCount <= 1)
        return;

    //We need to determine our target point. It may either be size (the total number of bytes,
    //summed across all files nested inside the renamed directories) or count (the number of
    //directories to rename).
    m_targetSize = 0;
    m_targetCount = 0;
    if (parameters.isFlag("size"))
        m_targetSize = m_rng.processFlag(parameters.getFlag("size"), m_manifest.getTotalSize());
    if (parameters.isFlag("count"))
    {
        m_targetCount = m_rng.processFlag(parameters.getFlag("count"), totalDirectoryCount);
        //The default is 10% count but make sure we always do at least 1
        if (m_targetCount == 0)
            m_targetCount = 1;
    }

    //We're going to iterate through the list of directories using the coprime stride method, the
    //same approach used elsewhere in this codebase to efficiently visit a pseudo-random subset
    //of the list, even if the user requests a high percentage.
    uint64_t dirPos = m_rng.getFromRange(1, totalDirectoryCount-1);
    uint64_t stride = (totalDirectoryCount <= 2) ? 1 : m_rng.getFromRange(1, totalDirectoryCount-1);
    //The stride must not have a common denominator compared to the size of the list
    while (std::gcd(stride, totalDirectoryCount) != 1) {
        stride = m_rng.getFromRange(1, totalDirectoryCount-1);
    }

    uint64_t sizeSoFar = 0;
    uint64_t countSoFar = 0;
    uint64_t iteratorCounter = 0;
    //Loop until either the target size or target count is reached
    while ((parameters.isFlag("size") && (sizeSoFar < m_targetSize))
           || (parameters.isFlag("count") && (countSoFar < m_targetCount)))
    {
        //Position 0 (the root directory) is never a valid target
        if (dirPos != 0)
        {
            DDDirectory &directory = m_manifest.getDirectoryByPos(dirPos);
            if (directory.processingStatus() == DDDirectory::NONE)
            {
                //Measure the directory before renaming it; the rename only changes paths, not sizes
                uint64_t directorySize = CalculateDirectorySize(directory.relativePath());
                RenameOneDirectory(directory, parameters);
                sizeSoFar += directorySize;
                m_processedSize += directorySize;
                countSoFar++;
                //Update the status every 5 directories
                if (countSoFar % 5 == 0)
                    UpdateProcessingStatus();
            }
        }
        // Jump forward by the stride and wrap around using modulo
        dirPos = (dirPos + stride) % totalDirectoryCount;
        iteratorCounter++;
        if (iteratorCounter > totalDirectoryCount)
            //Maybe user is trying to rename more directories than actually exist
            break;
    }
}

/**
 * @brief DDOperationRenameDirectory::RenameOneDirectory
 * Renames a single directory on the file system and then propagates that rename to every
 * file and subdirectory in the manifest that lived underneath it.
 * @param directory The directory to be renamed
 * @param parameters The parameters for this operation
 */
void DDOperationRenameDirectory::RenameOneDirectory(DDDirectory &directory, DDParameters &parameters)
{
    filesystem::path oldRelativePath = directory.relativePath();
    filesystem::path absoluteOldPath = parameters.ConvertToAbsolutePath(oldRelativePath);

    directory.setProcessingStatus(DDDirectory::PROCESSING);
    try
    {
        //The directory must actually exist
        if (!std::filesystem::exists(absoluteOldPath))
        {
            directory.recordProcessingError("Directory is missing from the file system: " + absoluteOldPath.string());
            return;
        }

        //Come up with a new name for the directory. It stays inside the same parent directory.
        filesystem::path parentPath = oldRelativePath.parent_path();
        string newName = "DD_" + m_rng.getSimpleString(10);
        filesystem::path newRelativePath = parentPath / newName;
        filesystem::path absoluteNewPath = parameters.ConvertToAbsolutePath(newRelativePath);

        //We don't want to overwrite an existing directory
        if (std::filesystem::exists(absoluteNewPath))
        {
            directory.setProcessingStatus(DDDirectory::CONFLICT);
            return;
        }

        //Renaming the directory on disk moves its entire subtree (files and subdirectories) at once
        std::filesystem::rename(absoluteOldPath, absoluteNewPath);

        //Update this directory's own manifest entry
        directory.setRelativePath(newRelativePath);

        //Now update every file and subdirectory in the manifest whose path descended from
        //the old directory path, since the physical rename moved them all underneath it.
        UpdateDescendantPaths(oldRelativePath, newRelativePath);

        m_processedCount++;
    }
    catch (const std::exception& e) {
        directory.recordProcessingError("Exception thrown when processing directory " + absoluteOldPath.string() + ": " + e.what());
        return;
    }
    catch(...)
    {
        directory.recordProcessingError("Unknown exception when processing directory " + absoluteOldPath.string());
        return;
    }
    directory.setProcessingStatus(DDDirectory::NONE);
}

/**
 * @brief DDOperationRenameDirectory::UpdateDescendantPaths
 * Walks every file and directory in the manifest and, for any whose relative path descended
 * from oldPath, rewrites it so that it descends from newPath instead.
 * @param oldPath The old directory path that was renamed
 * @param newPath The new directory path
 */
void DDOperationRenameDirectory::UpdateDescendantPaths(const filesystem::path &oldPath, const filesystem::path &newPath)
{
    //Update any subdirectories that lived underneath the renamed directory
    for (uint64_t dirLoop = 0; dirLoop < m_manifest.getTotalDirectoryCount(); dirLoop++)
    {
        DDDirectory &subDirectory = m_manifest.getDirectoryByPos(dirLoop);
        if (IsDescendantPath(subDirectory.relativePath(), oldPath))
            subDirectory.setRelativePath(ReplacePathPrefix(subDirectory.relativePath(), oldPath, newPath));
    }

    //Update any files that lived underneath the renamed directory
    for (uint64_t fileLoop = 0; fileLoop < m_manifest.getTotalFileCount(); fileLoop++)
    {
        DDFile &file = m_manifest.getFileByPos(fileLoop);
        if (IsDescendantPath(file.relativePathname(), oldPath))
            file.setRelativePathname(ReplacePathPrefix(file.relativePathname(), oldPath, newPath));
    }
}

/**
 * @brief DDOperationRenameDirectory::IsDescendantPath
 * Determines whether "path" lives underneath "ancestor" (or is equal to it), by comparing
 * path components rather than doing a raw string prefix comparison.
 */
bool DDOperationRenameDirectory::IsDescendantPath(const filesystem::path &path, const filesystem::path &ancestor)
{
    auto pathIt = path.begin();
    auto ancestorIt = ancestor.begin();
    for (; ancestorIt != ancestor.end(); ++ancestorIt, ++pathIt)
    {
        if ((pathIt == path.end()) || (*pathIt != *ancestorIt))
            return false;
    }
    return true;
}

/**
 * @brief DDOperationRenameDirectory::ReplacePathPrefix
 * Given a path that is known to descend from oldPrefix, returns a new path with oldPrefix
 * swapped out for newPrefix, keeping every path component after the prefix intact.
 */
filesystem::path DDOperationRenameDirectory::ReplacePathPrefix(const filesystem::path &path, const filesystem::path &oldPrefix, const filesystem::path &newPrefix)
{
    filesystem::path result = newPrefix;

    auto pathIt = path.begin();
    auto oldIt = oldPrefix.begin();
    //Skip past the shared prefix portion
    for (; oldIt != oldPrefix.end(); ++oldIt, ++pathIt) {}
    //Append whatever remains of the original path
    for (; pathIt != path.end(); ++pathIt)
        result /= *pathIt;

    return result;
}

/**
 * @brief DDOperationRenameDirectory::CalculateDirectorySize
 * A directory's size is the sum of the sizes of every file in the manifest that lives
 * underneath it, including files inside any subdirectories.
 * @param directoryPath The relative path of the directory to measure
 * @return The total size, in bytes, of all files nested inside that directory
 */
uint64_t DDOperationRenameDirectory::CalculateDirectorySize(const filesystem::path &directoryPath)
{
    uint64_t totalSize = 0;
    for (uint64_t fileLoop = 0; fileLoop < m_manifest.getTotalFileCount(); fileLoop++)
    {
        DDFile &file = m_manifest.getFileByPos(fileLoop);
        if (IsDescendantPath(file.relativePathname(), directoryPath))
            totalSize += file.size();
    }
    return totalSize;
}

string DDOperationRenameDirectory::GetOperationSummation()
{
    string summation;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(m_endProcessing - m_startProcessing).count();
    summation = std::format(std::locale(""), "{:L} directories renamed. {:L} bytes worth of files affected in {:.6Lf} seconds",
                            m_processedCount.load(), m_processedSize.load(), elapsedSeconds);
    return summation;
}

#include "DDOperationMoveDirectory.h"

#include <numeric>
#include <vector>
#include <format>
#include <locale>

using namespace std;

void DDOperationMoveDirectory::ChildSetDefaultParameters(DDParameters &parameters)
{
    //Either the size or the count must be filled in. A directory's "size" is the sum of the
    //sizes of every file nested underneath it.
    if (!parameters.isFlag("size") && !parameters.isFlag("count"))
    {
        //Move 10% as the default
        parameters.setFlag("count", "10%");
    }
}

void DDOperationMoveDirectory::ChildDoOperation(DDParameters &parameters)
{
    uint64_t totalDirectoryCount = m_manifest.getTotalDirectoryCount();
    m_filesAffected = 0;
    //Position 0 is always the root directory and it can never be moved (it has no parent of
    //its own to reassign), so there must be at least one other directory for this operation to
    //do anything.
    if (totalDirectoryCount <= 1)
        return;

    //We need to determine our target point. It may either be size (the total number of bytes,
    //summed across all files nested inside the moved directories) or count (the number of
    //directories to move).
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
                //MoveOneDirectory reports how many bytes worth of files it moved, and also
                //updates m_processedSize/m_processedCount itself
                sizeSoFar += MoveOneDirectory(directory, parameters);
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
            //Maybe user is trying to move more directories than actually exist
            break;
    }
}

/**
 * @brief DDOperationMoveDirectory::MoveOneDirectory
 * Moves a single directory to a different, randomly chosen parent directory - keeping its own
 * name, only its location changes - then propagates that move to every file and subdirectory
 * in the manifest that lived underneath it.
 * @param directory The directory to be moved
 * @param parameters The parameters for this operation
 * @return The total size, in bytes, of the files that were moved underneath the directory
 * (i.e. the directory's size). Returns 0 if the move did not happen.
 */
uint64_t DDOperationMoveDirectory::MoveOneDirectory(DDDirectory &directory, DDParameters &parameters)
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
            return 0;
        }

        //Pick a new parent directory for this one to live under. It can't be this directory
        //itself or any of its own descendants (that would nest it inside itself), and it can't
        //be its current parent (that wouldn't actually move it anywhere).
        filesystem::path currentParentPath = oldRelativePath.parent_path();
        vector<uint64_t> candidatePositions;
        candidatePositions.reserve(m_manifest.getTotalDirectoryCount());
        for (uint64_t dirLoop = 0; dirLoop < m_manifest.getTotalDirectoryCount(); dirLoop++)
        {
            DDDirectory &candidate = m_manifest.getDirectoryByPos(dirLoop);
            if (IsDescendantPath(candidate.relativePath(), oldRelativePath))
                continue; //this is the directory itself, or one of its own descendants
            if (candidate.relativePath() == currentParentPath)
                continue; //this is where the directory already lives
            candidatePositions.push_back(dirLoop);
        }
        if (candidatePositions.empty())
        {
            directory.recordProcessingError("No valid destination directory found for " + absoluteOldPath.string());
            return 0;
        }
        DDDirectory &newParentDirectory = m_manifest.getDirectoryByPos(candidatePositions[m_rng.getFromRange(0, candidatePositions.size()-1)]);

        //The directory keeps its own name; only its parent changes
        filesystem::path newRelativePath = newParentDirectory.relativePath() / oldRelativePath.filename();
        filesystem::path absoluteNewPath = parameters.ConvertToAbsolutePath(newRelativePath);

        //We don't want to overwrite an existing directory
        if (std::filesystem::exists(absoluteNewPath))
        {
            directory.setProcessingStatus(DDDirectory::CONFLICT);
            return 0;
        }

        //Moving the directory on disk moves its entire subtree (files and subdirectories) at once
        std::filesystem::rename(absoluteOldPath, absoluteNewPath);

        //Update this directory's own manifest entry
        directory.setRelativePath(newRelativePath);

        //Now update every file and subdirectory in the manifest whose path descended from
        //the old directory path, since the physical move relocated them all underneath it.
        //This also tells us the directory's size, since that's just the sum of its files.
        uint64_t directorySize = UpdateDescendantPaths(oldRelativePath, newRelativePath);

        m_processedCount++;
        m_processedSize += directorySize;
        directory.setProcessingStatus(DDDirectory::NONE);
        return directorySize;
    }
    catch (const std::exception& e) {
        directory.recordProcessingError("Exception thrown when processing directory " + absoluteOldPath.string() + ": " + e.what());
        return 0;
    }
    catch(...)
    {
        directory.recordProcessingError("Unknown exception when processing directory " + absoluteOldPath.string());
        return 0;
    }
}

/**
 * @brief DDOperationMoveDirectory::UpdateDescendantPaths
 * Walks every file and directory in the manifest and, for any whose relative path descended
 * from oldPath, rewrites it so that it descends from newPath instead. While walking the files,
 * it also totals up their size - that total is the moved directory's size.
 * @param oldPath The old directory path that was moved
 * @param newPath The new directory path
 * @return The combined size, in bytes, of every file found underneath oldPath that had not
 * already been counted (a file can live underneath more than one directory that gets moved in
 * the same run, so it should only ever be counted once).
 */
uint64_t DDOperationMoveDirectory::UpdateDescendantPaths(const filesystem::path &oldPath, const filesystem::path &newPath)
{
    //Update any subdirectories that lived underneath the moved directory
    for (uint64_t dirLoop = 0; dirLoop < m_manifest.getTotalDirectoryCount(); dirLoop++)
    {
        DDDirectory &subDirectory = m_manifest.getDirectoryByPos(dirLoop);
        if (IsDescendantPath(subDirectory.relativePath(), oldPath))
            subDirectory.setRelativePath(ReplacePathPrefix(subDirectory.relativePath(), oldPath, newPath));
    }

    //Update any files that lived underneath the moved directory. A file can be a descendant of
    //more than one directory that gets moved during this operation (e.g. a parent directory and
    //one of its own nested subdirectories both get selected), so its path may legitimately need
    //updating more than once - but its size must only ever be counted once, or the reported total
    //could exceed the manifest's real total. We use the file's own processing status as a one-time
    //marker for that: it starts out as NONE, and we flip it to COMPLETE the first time it's counted.
    uint64_t totalSize = 0;
    for (uint64_t fileLoop = 0; fileLoop < m_manifest.getTotalFileCount(); fileLoop++)
    {
        DDFile &file = m_manifest.getFileByPos(fileLoop);
        if (IsDescendantPath(file.relativePathname(), oldPath))
        {
            if (file.processingStatus() == DDFile::NONE)
            {
                totalSize += file.size();
                m_filesAffected++;
                file.setProcessingStatus(DDFile::COMPLETE);
            }
            file.setRelativePathname(ReplacePathPrefix(file.relativePathname(), oldPath, newPath));
        }
    }
    return totalSize;
}

/**
 * @brief DDOperationMoveDirectory::IsDescendantPath
 * Determines whether "path" lives underneath "ancestor" (or is equal to it), by comparing
 * path components rather than doing a raw string prefix comparison.
 */
bool DDOperationMoveDirectory::IsDescendantPath(const filesystem::path &path, const filesystem::path &ancestor)
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
 * @brief DDOperationMoveDirectory::ReplacePathPrefix
 * Given a path that is known to descend from oldPrefix, returns a new path with oldPrefix
 * swapped out for newPrefix, keeping every path component after the prefix intact.
 */
filesystem::path DDOperationMoveDirectory::ReplacePathPrefix(const filesystem::path &path, const filesystem::path &oldPrefix, const filesystem::path &newPrefix)
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

string DDOperationMoveDirectory::GetOperationSummation()
{
    string summation;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(m_endProcessing - m_startProcessing).count();
    double writeSpeed = (elapsedSeconds > 0) ? (m_processedSize / elapsedSeconds) : 0.0;
    summation = std::format(std::locale(""), "{:L} directories moved, affecting {:L} files. {:L} bytes affected in {:.6Lf} seconds ({:.2Lf} bytes per second)",
                            m_processedCount.load(), m_filesAffected, m_processedSize.load(), elapsedSeconds, writeSpeed);
    return summation;
}

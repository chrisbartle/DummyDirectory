#include "DDOperationMoveDirectory.h"

#include <vector>
#include <utility>
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

    //Determine the maximum depth of the directory structure, same default as DDOperationAddDirectory
    uint64_t maxDepth = 2;
    if (parameters.isFlag("maxdepth"))
        maxDepth = stoul(parameters.getFlag("maxdepth"));

    if (maxDepth == 0)
        throw runtime_error("It is not possible to move directories if the maximum depth is set to 0");

    //Rather than picking directories at random and simply giving up on the ones that turn out
    //to have nowhere valid to go, first work out which directories can actually be moved
    //somewhere under the current constraints (maxdepth in particular can rule out a lot of
    //them), and only ever select from among those.
    vector<uint64_t> movablePositions;
    movablePositions.reserve(totalDirectoryCount);
    for (uint64_t dirLoop = 1; dirLoop < totalDirectoryCount; dirLoop++)
    {
        DDDirectory &candidate = m_manifest.getDirectoryByPos(dirLoop);
        uint64_t subtreeExtraDepth = GetSubtreeMaxDepth(candidate.relativePath(), candidate.getRelativePathDepth());
        if (!FindCandidateDestinations(candidate.relativePath(), subtreeExtraDepth, maxDepth).empty())
            movablePositions.push_back(dirLoop);
    }
    //Nothing in the manifest can be moved anywhere under these constraints
    if (movablePositions.empty())
        return;

    //Shuffle the movable directories into a random order (Fisher-Yates), so we can walk through
    //them without repeats and without ever needing to reject-and-retry on a directory that has
    //nowhere to go.
    for (uint64_t i = movablePositions.size() - 1; i > 0; i--)
    {
        uint64_t j = m_rng.getFromRange(0, i);
        std::swap(movablePositions[i], movablePositions[j]);
    }

    uint64_t sizeSoFar = 0;
    uint64_t countSoFar = 0;
    uint64_t cursor = 0;
    //Loop until either the target size or target count is reached, or we run out of movable
    //directories to try (a move can, rarely, still fail here - see MoveOneDirectory - if an
    //earlier move in this same run changed the depth of what had been this directory's only
    //valid destination).
    while (((parameters.isFlag("size") && (sizeSoFar < m_targetSize))
            || (parameters.isFlag("count") && (countSoFar < m_targetCount)))
           && (cursor < movablePositions.size()))
    {
        DDDirectory &directory = m_manifest.getDirectoryByPos(movablePositions[cursor]);
        if (directory.processingStatus() == DDDirectory::NONE)
        {
            //MoveOneDirectory reports how many bytes worth of files it moved, and also
            //updates m_processedSize/m_processedCount itself
            sizeSoFar += MoveOneDirectory(directory, parameters, maxDepth);
            countSoFar++;
            //Update the status every 5 directories
            if (countSoFar % 5 == 0)
                UpdateProcessingStatus();
        }
        cursor++;
    }
}

/**
 * @brief DDOperationMoveDirectory::MoveOneDirectory
 * Moves a single directory to a different, randomly chosen parent directory - keeping its own
 * name, only its location changes - then propagates that move to every file and subdirectory
 * in the manifest that lived underneath it.
 * @param directory The directory to be moved
 * @param parameters The parameters for this operation
 * @param maxDepth The deepest any directory is allowed to end up at, from the --maxdepth flag
 * @return The total size, in bytes, of the files that were moved underneath the directory
 * (i.e. the directory's size). Returns 0 if the move did not happen.
 */
uint64_t DDOperationMoveDirectory::MoveOneDirectory(DDDirectory &directory, DDParameters &parameters, uint64_t maxDepth)
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

        //The directory being moved might already have its own subtree of subdirectories
        //nested underneath it, so moving it isn't just about the directory's own depth - we
        //need to know how much deeper its existing subtree extends, so that wherever we put it,
        //none of its descendants end up past maxDepth either.
        uint64_t subtreeExtraDepth = GetSubtreeMaxDepth(oldRelativePath, directory.getRelativePathDepth());

        //Pick a new parent directory for this one to live under. It can't be this directory
        //itself or any of its own descendants (that would nest it inside itself), it can't be
        //its current parent (that wouldn't actually move it anywhere), and placing the
        //directory - along with its existing subtree - underneath it must not push any part of
        //it deeper than maxDepth.
        vector<uint64_t> candidatePositions = FindCandidateDestinations(oldRelativePath, subtreeExtraDepth, maxDepth);
        if (candidatePositions.empty())
        {
            //This can happen even though ChildDoOperation only selects from directories that
            //were pre-screened as movable, if an earlier move in this same run changed the
            //depth of what had been this directory's only valid destination.
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
 * @brief DDOperationMoveDirectory::GetSubtreeMaxDepth
 * Determines how many levels deep the deepest descendant of a directory lives, relative to
 * the directory itself. A directory with no subdirectories underneath it returns 0.
 * @param directoryPath The relative path of the directory being measured
 * @param directoryDepth That directory's own depth, i.e. its getRelativePathDepth()
 * @return The relative depth of the deepest nested subdirectory
 */
uint64_t DDOperationMoveDirectory::GetSubtreeMaxDepth(const filesystem::path &directoryPath, uint64_t directoryDepth)
{
    uint64_t maxExtraDepth = 0;
    for (uint64_t dirLoop = 0; dirLoop < m_manifest.getTotalDirectoryCount(); dirLoop++)
    {
        DDDirectory &candidate = m_manifest.getDirectoryByPos(dirLoop);
        if (!IsDescendantPath(candidate.relativePath(), directoryPath))
            continue;
        uint64_t extraDepth = candidate.getRelativePathDepth() - directoryDepth;
        if (extraDepth > maxExtraDepth)
            maxExtraDepth = extraDepth;
    }
    return maxExtraDepth;
}

/**
 * @brief DDOperationMoveDirectory::FindCandidateDestinations
 * Finds every directory in the manifest that directoryPath could validly be moved underneath:
 * not itself or one of its own descendants (that would nest it inside itself), not its current
 * parent (that wouldn't actually move it anywhere), and not so shallow-relative-to-its-subtree
 * that any part of it would end up deeper than maxDepth.
 * @param directoryPath The relative path of the directory being moved
 * @param subtreeExtraDepth How many levels deeper the directory's existing subtree extends,
 * from GetSubtreeMaxDepth()
 * @param maxDepth The deepest any directory is allowed to end up at, from the --maxdepth flag
 * @return The manifest positions of every valid destination directory (may be empty)
 */
vector<uint64_t> DDOperationMoveDirectory::FindCandidateDestinations(const filesystem::path &directoryPath, uint64_t subtreeExtraDepth, uint64_t maxDepth)
{
    filesystem::path currentParentPath = directoryPath.parent_path();
    vector<uint64_t> candidatePositions;
    candidatePositions.reserve(m_manifest.getTotalDirectoryCount());
    for (uint64_t dirLoop = 0; dirLoop < m_manifest.getTotalDirectoryCount(); dirLoop++)
    {
        DDDirectory &candidate = m_manifest.getDirectoryByPos(dirLoop);
        if (IsDescendantPath(candidate.relativePath(), directoryPath))
            continue; //this is the directory itself, or one of its own descendants
        if (candidate.relativePath() == currentParentPath)
            continue; //this is where the directory already lives
        if (candidate.getRelativePathDepth() + 1 + subtreeExtraDepth > maxDepth)
            continue; //the moved directory's deepest descendant would end up too deep
        candidatePositions.push_back(dirLoop);
    }
    return candidatePositions;
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

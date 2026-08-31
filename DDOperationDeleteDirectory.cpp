#include "DDOperationDeleteDirectory.h"

#include <numeric>
#include <algorithm>
#include <vector>
#include <format>
#include <locale>

using namespace std;

void DDOperationDeleteDirectory::ChildSetDefaultParameters(DDParameters &parameters)
{
    //Either the size or the count must be filled in. A directory's "size" is the sum of the
    //sizes of every file nested underneath it.
    if (!parameters.isFlag("size") && !parameters.isFlag("count"))
    {
        //Delete 10% as the default
        parameters.setFlag("count", "10%");
    }
}

void DDOperationDeleteDirectory::ChildDoOperation(DDParameters &parameters)
{
    //QueueDirectoryForDeletion locates a claimed directory's descendants with a binary search,
    //which requires the manifest to already be sorted by path. It normally already is - every
    //operation ends with PostOperationCleanup(), which sorts - but we make sure of it here, the
    //same way DDOperationClean does.
    m_manifest.Sort();

    uint64_t totalDirectoryCount = m_manifest.getTotalDirectoryCount();
    m_filesAffected = 0;
    //Position 0 is always the root directory and it can never be deleted, so there must be
    //at least one other directory for this operation to do anything.
    if (totalDirectoryCount <= 1)
        return;

    //We need to determine our target point. It may either be size (the total number of bytes,
    //summed across all files nested inside the deleted directories) or count (the number of
    //directories to delete).
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

    //Keep track of which directories were selected at the top level (as opposed to merely being
    //swept up as a descendant of one), so we can report how many completed successfully once
    //everything - including all the threaded file deletions - is done.
    vector<uint64_t> selectedPositions;

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
            //A directory that isn't NONE anymore was already claimed by an earlier pick in this
            //same run (its parent, or an ancestor further up, was already selected) - skip it
            //rather than processing the same subtree twice.
            if (directory.processingStatus() == DDDirectory::NONE)
            {
                //Claims this directory and its descendants, and queues up deletion of every
                //file underneath it on the thread pool
                sizeSoFar += QueueDirectoryForDeletion(directory, parameters);
                selectedPositions.push_back(dirPos);
                countSoFar++;
                if (!parameters.isFlag("size"))
                    //We're driven by --count here, so m_targetSize isn't being used to decide
                    //when to stop selecting - repurpose it to track the running total of bytes
                    //queued so far. The actual deletions happen on background threads and only
                    //update m_processedSize (not m_processedCount, which this class deliberately
                    //reserves for completed directories - see DDOperationDeleteDirectory.h), and
                    //m_processedCount doesn't move until every file is gone and every claimed
                    //directory has been removed at the very end of this function. Without this,
                    //UpdateProcessingStatus() below (and the percentage shown to the user, via
                    //WaitForThreadsToComplete()'s polling once this loop finishes) would sit at
                    //0% for the entire, and usually by far longest, file-deletion phase and only
                    //jump once everything is already done.
                    m_targetSize = sizeSoFar;
                //Update the status every 5 directories
                if (countSoFar % 5 == 0)
                    UpdateProcessingStatus();
            }
        }
        // Jump forward by the stride and wrap around using modulo
        dirPos = (dirPos + stride) % totalDirectoryCount;
        iteratorCounter++;
        if (iteratorCounter > totalDirectoryCount)
            //Maybe user is trying to delete more directories than actually exist
            break;
    }

    //Let the thread pool finish deleting every queued file before we try removing any
    //directories - a directory can only be removed once it's actually empty.
    WaitForThreadsToComplete();

    //Now remove the directories themselves: every directory we claimed above (the selected
    //directories and all of their nested subdirectories), deepest first, so a parent is always
    //empty by the time we reach it.
    RemoveClaimedDirectories(parameters);

    //Only count a selected directory as fully deleted if it actually came out the other end
    for (uint64_t pos : selectedPositions)
    {
        if (m_manifest.getDirectoryByPos(pos).processingStatus() == DDDirectory::DELETED)
            m_processedCount++;
    }
}

/**
 * @brief DDOperationDeleteDirectory::QueueDirectoryForDeletion
 * Claims this directory, and every subdirectory nested inside it, by marking them PROCESSING
 * so the selection loop won't pick them again, then queues a deletion task on the thread pool
 * for every file underneath it that hasn't already been claimed by an overlapping selection
 * (a parent directory and one of its own nested subdirectories can both get picked in the same
 * run - only the first one to reach a given file should queue it).
 * @param directory The top-level directory that was selected
 * @param parameters The parameters for this operation
 * @return The anticipated size, in bytes, of the files just queued for deletion
 */
uint64_t DDOperationDeleteDirectory::QueueDirectoryForDeletion(DDDirectory &directory, DDParameters &parameters)
{
    filesystem::path directoryPath = directory.relativePath();

    //Unlike rename/move, nothing here ever changes a surviving entry's path - directories and
    //files are only ever marked with a new processing status, never given a new path - so the
    //manifest's sort order can never drift over the course of this operation. That means a
    //binary search here is always safe, with no need to ever re-sort or relocate anything
    //afterward, and it turns what used to be a full scan of the manifest per claimed directory
    //into a jump straight to the start of the actual run of descendants.

    //Claim this directory and every subdirectory nested inside it
    uint64_t totalDirectories = m_manifest.getTotalDirectoryCount();
    uint64_t dirLow = 0, dirHigh = totalDirectories;
    while (dirLow < dirHigh)
    {
        uint64_t mid = dirLow + (dirHigh - dirLow) / 2;
        if (m_manifest.getDirectoryByPos(mid).relativePath() < directoryPath)
            dirLow = mid + 1;
        else
            dirHigh = mid;
    }
    for (uint64_t dirLoop = dirLow; (dirLoop < totalDirectories) && IsDescendantPath(m_manifest.getDirectoryByPos(dirLoop).relativePath(), directoryPath); dirLoop++)
        m_manifest.getDirectoryByPos(dirLoop).setProcessingStatus(DDDirectory::PROCESSING);

    //Queue up deletion of every unclaimed file underneath this directory
    uint64_t totalFiles = m_manifest.getTotalFileCount();
    uint64_t fileLow = 0, fileHigh = totalFiles;
    while (fileLow < fileHigh)
    {
        uint64_t mid = fileLow + (fileHigh - fileLow) / 2;
        if (m_manifest.getFileByPos(mid).relativePathname() < directoryPath)
            fileLow = mid + 1;
        else
            fileHigh = mid;
    }
    uint64_t anticipatedSize = 0;
    for (uint64_t fileLoop = fileLow; (fileLoop < totalFiles) && IsDescendantPath(m_manifest.getFileByPos(fileLoop).relativePathname(), directoryPath); fileLoop++)
    {
        DDFile &file = m_manifest.getFileByPos(fileLoop);
        if (file.processingStatus() == DDFile::NONE)
        {
            anticipatedSize += file.size();
            file.setProcessingStatus(DDFile::QUEUED);
            DoFileOperation(file, parameters, 0, 0);
        }
    }
    return anticipatedSize;
}

/**
 * @brief DDOperationDeleteDirectory::ChildDoFileOperation
 * Deletes a single file. We only ever remove files that the manifest is actually tracking -
 * never anything else that might happen to be sitting alongside them - which is why this
 * operation deletes files one at a time rather than recursively wiping out a directory's
 * entire contents.
 */
void DDOperationDeleteDirectory::ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size)
{
    //We can only process queued items
    if (file.processingStatus() != DDFile::QUEUED)
        return;

    filesystem::path absolutePathname = parameters.ConvertToAbsolutePath(file.relativePathname());

    //Mark that this file is being processed
    file.setProcessingStatus(DDFile::STARTED);
    try
    {
        //If the file was already gone (e.g. removed some other way between the manifest being
        //loaded and now), treat that as already handled rather than an error.
        if (std::filesystem::remove(absolutePathname))
        {
            m_processedSize += file.size();
            m_filesAffected++;
        }
        file.setProcessingStatus(DDFile::DELETED);
    }
    catch (const std::exception& e) {
        file.recordProcessingError("Exception thrown when deleting file " + absolutePathname.string() + ": " + e.what());
        return;
    }
    catch(...)
    {
        file.recordProcessingError("Unknown exception when deleting file " + absolutePathname.string());
        return;
    }
}

/**
 * @brief DDOperationDeleteDirectory::RemoveClaimedDirectories
 * Removes every directory in the manifest that was claimed by this operation (marked
 * PROCESSING by QueueDirectoryForDeletion) from the file system. Directories are processed
 * deepest-first so that a parent is always empty - all of its files having already been
 * deleted by the thread pool, and all of its subdirectories already removed by this same loop
 * - by the time we attempt to remove it. Removing a directory only succeeds if it's actually
 * empty, so if something this tool doesn't own is still sitting inside one (or one of its
 * files failed to delete above), that directory - and by extension its ancestors - is simply
 * left in place rather than forced out.
 * @param parameters The parameters for this operation
 */
void DDOperationDeleteDirectory::RemoveClaimedDirectories(DDParameters &parameters)
{
    //Gather every directory that was claimed during selection
    std::vector<uint64_t> claimedPositions;
    for (uint64_t dirLoop = 0; dirLoop < m_manifest.getTotalDirectoryCount(); dirLoop++)
    {
        if (m_manifest.getDirectoryByPos(dirLoop).processingStatus() == DDDirectory::PROCESSING)
            claimedPositions.push_back(dirLoop);
    }

    //Sort deepest-first so subdirectories are always removed before their parents
    std::sort(claimedPositions.begin(), claimedPositions.end(), [this](uint64_t a, uint64_t b) {
        return m_manifest.getDirectoryByPos(a).getRelativePathDepth() > m_manifest.getDirectoryByPos(b).getRelativePathDepth();
    });

    for (uint64_t pos : claimedPositions)
    {
        DDDirectory &directory = m_manifest.getDirectoryByPos(pos);
        filesystem::path absolutePath = parameters.ConvertToAbsolutePath(directory.relativePath());
        try
        {
            if (!std::filesystem::exists(absolutePath))
            {
                //Already gone - nothing more to do here
                directory.setProcessingStatus(DDDirectory::DELETED);
                continue;
            }
            //This only succeeds if the directory is now empty. If it isn't - because it holds
            //something we don't own, or one of its files failed to delete above - this throws,
            //and we leave the directory alone rather than forcing it out.
            std::filesystem::remove(absolutePath);
            directory.setProcessingStatus(DDDirectory::DELETED);
        }
        catch (const std::exception& e) {
            directory.recordProcessingError("Exception thrown when removing directory " + absolutePath.string() + ": " + e.what());
        }
        catch (...) {
            directory.recordProcessingError("Unknown exception when removing directory " + absolutePath.string());
        }
    }
}

/**
 * @brief DDOperationDeleteDirectory::IsDescendantPath
 * Determines whether "path" lives underneath "ancestor" (or is equal to it), by comparing
 * path components rather than doing a raw string prefix comparison.
 */
bool DDOperationDeleteDirectory::IsDescendantPath(const filesystem::path &path, const filesystem::path &ancestor)
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

string DDOperationDeleteDirectory::GetOperationSummation()
{
    string summation;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(m_endProcessing - m_startProcessing).count();
    double removeSpeed = (elapsedSeconds > 0) ? (m_processedSize / elapsedSeconds) : 0.0;
    summation = std::format(std::locale(""), "{:L} directories deleted, affecting {:L} files. {:L} bytes removed in {:.6Lf} seconds\n({:.2Lf} bytes per second)",
                            m_processedCount.load(), m_filesAffected.load(), m_processedSize.load(), elapsedSeconds, removeSpeed);
    return summation;
}

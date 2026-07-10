#include "DDOperationRename.h"

#include <numeric>

void DDOperationRename::ChildSetDefaultParameters(DDParameters &parameters)
{
    //Either the size or the count must be filled in
    if (!parameters.isFlag("size") && !parameters.isFlag("count"))
    {
        //Rename 10% as the default
        parameters.setFlag("count", "10%");
    }
}

void DDOperationRename::ChildDoOperation(DDParameters &parameters)
{
    uint64_t totalFileCount = m_manifest.getTotalFileCount();
    if (totalFileCount == 0)
        return;

    //We need to determine our target point. It may either be size (the total number of bytes to be delete)
    //or count (the total number of objects to be renamed)
    m_targetSize = 0;
    m_targetCount = 0;
    if (parameters.isFlag("size"))
        m_targetSize = m_rng.processFlag(parameters.getFlag("size"), m_manifest.getTotalSize());
    if (parameters.isFlag("count"))
    {
        m_targetCount = m_rng.processFlag(parameters.getFlag("count"), m_manifest.getTotalFileCount());
        //The default is 10% count but make sure we always do at least 1
        if (m_targetCount == 0)
            m_targetCount = 1;
    }

    //We're going to iterate through the list of files using the coprime stride method. This will guarantee
    //that we efficiently delete the correct number of files even if the user requests a high percentage (90%)
    uint64_t filePos = m_rng.getFromRange(0, totalFileCount-1);
    uint64_t stride = (totalFileCount == 1) ? 1 : m_rng.getFromRange(1, totalFileCount-1);
    //The stride must not have a common denominator compared to the size of the list
    while (std::gcd(stride, totalFileCount) != 1) {
        stride = m_rng.getFromRange(1, totalFileCount-1);
    }
    uint64_t sizeSoFar = 0;
    uint64_t countSoFar = 0;
    uint64_t iteratorCounter = 0;
    //Loop until either the target size or target count is reached
    while ((parameters.isFlag("size") && (sizeSoFar < m_targetSize))
           || (parameters.isFlag("count") && (countSoFar < m_targetCount)))
    {
        DDFile& file = m_manifest.getFileByPos(filePos);
        if (file.processingStatus() == DDFile::NONE)
        {
            sizeSoFar += file.size();
            countSoFar++;
            file.setProcessingStatus(DDFile::QUEUED);
            DoFileOperation(file, parameters, 0, 0);
        }
        // Jump forward by the stride and wrap around using modulo
        filePos = (filePos + stride) % totalFileCount;
        iteratorCounter++;
        if (iteratorCounter > totalFileCount)
            //Maybe user is trying to rename 110%?
            break;
    }
}

void DDOperationRename::ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size)
{
    //We can only processed queued items
    if (file.processingStatus() != DDFile::QUEUED)
        return;

    filesystem::path absolutePathname = parameters.ConvertToAbsolutePath(file.relativePathname());

    //Mark that this file is being processed
    file.setProcessingStatus(DDFile::STARTED);
    try
    {
        filesystem::path oldFilepath = file.relativePathname();
        filesystem::path newFilepath = file.relativePathname();
        string newFilename = "DD_" + m_rng.getSimpleString(10) + oldFilepath.extension().string();
        newFilepath.replace_filename(newFilename);
        filesystem::path absoluteNewPathname = parameters.ConvertToAbsolutePath(newFilepath);
        //Make sure the file is there
        if (!std::filesystem::exists(absolutePathname))
        {
            file.setProcessingStatus(DDFile::MISSING);
            return;
        }

        //Rename the file
        filesystem::rename(absolutePathname, absoluteNewPathname);
        file.setRelativePathname(newFilepath);
        m_processedCount++;
        m_processedSize += file.size();
    }
    catch (const std::exception& e) {
        file.recordProcessingError("Exception thrown when processing file " + absolutePathname.string() + ": " + e.what());
        return;
    }
    catch(...)
    {
        file.recordProcessingError("Unknown exception when processing file " + absolutePathname.string());
        return;
    }
    file.setProcessingStatus(DDFile::COMPLETE);
}

string DDOperationRename::GetOperationSummation()
{
    string summation;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(m_endProcessing - m_startProcessing).count();
    double writeSpeed = (elapsedSeconds > 0) ? (m_processedSize / elapsedSeconds) : 0.0;
    summation = std::format(std::locale(""), "{:L} items renamed. {:L} bytes renamed in {:.6Lf} seconds ({:.2Lf} bytes per second)",
                            m_processedCount.load(), m_processedSize.load(), elapsedSeconds, writeSpeed);
    return summation;
}

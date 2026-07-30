#include "DDOperationClean.h"
#include <fstream>

using namespace std;

void DDOperationClean::ChildDoOperation(DDParameters &parameters)
{
    //Set expectations
    m_targetCount = m_manifest.getTotalFileCount();
    m_targetCount += m_manifest.getTotalDirectoryCount();

    //Start out by deleting every file in the manifest. This can be done very quickly
    for (uint64_t filePos = 0; (filePos < m_manifest.getTotalFileCount()); filePos++)
    {
        DDFile& file = m_manifest.getFileByPos(filePos);
        file.setProcessingStatus(DDFile::QUEUED);
        DoFileOperation(file, parameters, 0, 0);
    }
    WaitForThreadsToComplete();

    //The next step is to iterate through the entire directory structure and
    //remove all directories and files that match the DD_* pattern
    //vector<DDFile> fileQueue;
    vector<filesystem::path> directoriesToDeleteLater;
    std::vector<std::unique_ptr<DDFile>> filesToDelete;
    for (auto it = filesystem::recursive_directory_iterator(parameters.getAbsoluteDirectoryPath()); it != filesystem::recursive_directory_iterator(); ++it)
    {
        const auto& entry = *it;
        //Delete any files that start with DD_
        if (filesystem::is_regular_file(entry.path()) && entry.path().filename().string().starts_with("DD_"))
        {
            filesToDelete.emplace_back(std::make_unique<DDFile>());
            DDFile& file = *(filesToDelete.back());
            file.setRelativePathname(entry.path());
            file.setProcessingStatus(DDFile::QUEUED);
            DoFileOperation(file, parameters, 0, 1);
            m_targetCount++;
        }
        else if (filesystem::is_directory(entry.path()))
        {
            if (entry.path().filename().string().starts_with("DD_"))
            {
                directoriesToDeleteLater.push_back(entry.path());
                m_targetCount++;
            }
            else
                it.disable_recursion_pending();
        }
    }
    WaitForThreadsToComplete();
    //Iterate backwards through all of the directories we found and remove them one by one
    for (auto it = directoriesToDeleteLater.rbegin(); it != directoriesToDeleteLater.rend(); ++it)
    {
        filesystem::path& dirPath = *it;
        if (filesystem::is_empty(dirPath))
        {
            filesystem::remove(dirPath);
            m_processedCount++;
        }
    }

    //Finally, remove every directory. We'll do this in reverse order and because
    //of how the directories are sorted the subdirectories should always be processed
    //before the parent.
    //The above code probably already removed any empty directories but this code will
    //execute fine anyway.
    for (uint64_t dirPos = m_manifest.getTotalDirectoryCount()-1; (dirPos > 0); dirPos--)
    {
        DDDirectory& directory = m_manifest.getDirectoryByPos(dirPos);
        filesystem::path absolutePath = parameters.ConvertToAbsolutePath(directory.relativePath());
        try {
            filesystem::remove(absolutePath);
            directory.setProcessingStatus(DDDirectory::DELETED);
        } catch (...) {
            //Ignore failure here, just go forward
        }
    }
}

void DDOperationClean::ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size)
{
    //We can only processed queued items
    if (file.processingStatus() != DDFile::QUEUED)
        return;

    //Mark that this file is being processed
    file.setProcessingStatus(DDFile::STARTED);
    try
    {
        filesystem::path absolutePathname;
        //Special processing. If size = 1 then that's an indicator that this file is not part of the manifest.
        if (size == 1)
        {
            absolutePathname = file.relativePathname();
            //We need to confirm that this is indeed a DummyDirectory file. We do this by opening it and confirming
            //the file's prefix
            int prefixLength = m_filePrefix.length();
            std::ifstream inFile(absolutePathname, std::ios::binary);
            std::vector<char> buffer(prefixLength);
            inFile.read(buffer.data(), prefixLength);
            if ((inFile.gcount() != prefixLength) || !std::equal(m_filePrefix.begin(), m_filePrefix.end(), buffer.begin()))
                return;
        }
        else
            absolutePathname = parameters.ConvertToAbsolutePath(file.relativePathname());

        //Delete the file
        filesystem::remove(absolutePathname);
        file.setProcessingStatus(DDFile::DELETED);
        m_processedCount++;
    }
    catch (const std::exception& e) {
        file.recordProcessingError("Exception thrown when processing file " + file.relativePathname().string() + ": " + e.what());
        return;
    }
    catch(...)
    {
        file.recordProcessingError("Unknown exception when processing file " + file.relativePathname().string());
        return;
    }
}

string DDOperationClean::GetOperationSummation()
{
    string summation;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(m_endProcessing - m_startProcessing).count();
    summation = std::format(std::locale(""), "{:L} items removed in {:.6Lf} seconds",
                            m_processedCount.load(), elapsedSeconds);
    return summation;
}

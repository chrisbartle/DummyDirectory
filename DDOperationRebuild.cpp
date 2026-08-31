#include "DDOperationRebuild.h"
#include "DDMD5Hasher.h"

#include <fstream>

using namespace std;

void DDOperationRebuild::ChildDoOperation(DDParameters &parameters)
{
    uint64_t loops = 0;
    //Iterate through the target directory
    for (auto it = filesystem::recursive_directory_iterator(parameters.getAbsoluteDirectoryPath()); it != filesystem::recursive_directory_iterator(); ++it)
    {
        const auto& entry = *it;
        //Add any files that start with DD_
        if (filesystem::is_regular_file(entry.path()) && entry.path().filename().string().starts_with("DD_"))
        {
            filesystem::path relativePath = filesystem::relative(entry.path(), parameters.getAbsoluteDirectoryPath());
            DDFile& file = m_manifest.addFile();
            file.setRelativePathname(relativePath);
            file.setSize(entry.file_size());
            m_targetSize += entry.file_size();
            m_targetCount++;
            file.setProcessingStatus(DDFile::QUEUED);
            DoFileOperation(file, parameters, 0, entry.file_size());
        }
        else if (filesystem::is_directory(entry.path()))
        {
            if (entry.path().filename().string().starts_with("DD_"))
            {
                filesystem::path relativePath = filesystem::relative(entry.path(), parameters.getAbsoluteDirectoryPath());
                DDDirectory& directory = m_manifest.addDirectory();
                directory.setRelativePath(relativePath);
                m_targetCount++;
                m_processedCount++;
            }
            else
                //Don't iterate into directories that don't start with DD_
                it.disable_recursion_pending();
        }

        //Sometimes this loop code is slower than the file operation threads so we should call
        //the visual refresh every 1000 items or so
        loops++;
        if (loops % 1000 == 0)
            UpdateProcessingStatus();
    }
}

void DDOperationRebuild::ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size)
{
    //We can only processed queued items
    if (file.processingStatus() != DDFile::QUEUED)
        return;

    filesystem::path absolutePathname = parameters.ConvertToAbsolutePath(file.relativePathname());

    //Mark that this file is being processed
    file.setProcessingStatus(DDFile::STARTED);
    try
    {
        //The file must exist
        if (!std::filesystem::exists(absolutePathname))
        {
            file.setProcessingStatus(DDFile::MISSING);
            return;
        }

        //Open the file
        std::ifstream inFile(absolutePathname, std::ios::binary);

        //Iterate through the file data and build the hash
        DDMD5Hasher hasher;
        char buffer[BUFFER_SIZE];
        inFile.read(buffer, BUFFER_SIZE);
        //Verify that the prefix matches what we expect. A file too short to even hold the prefix
        //cannot be one of ours either - checking the length as part of the match rather than as a
        //precondition for it, so that short files are rejected instead of silently adopted.
        if ((static_cast<size_t>(inFile.gcount()) < m_filePrefix.length()) || (string_view(buffer, m_filePrefix.length()) != m_filePrefix))
        {
            //This file was not created by this software and should not be added to the manifest
            file.setProcessingStatus(DDFile::DELETED);
            m_processedCount++;
            return;
        }
        while (inFile.gcount() > 0)
        {
            hasher.update(buffer, inFile.gcount());
            m_processedSize += inFile.gcount();
            inFile.read(buffer, BUFFER_SIZE);
        }

        //Finalize the hash
        std::string thisHash = hasher.finalize();
        file.setHash(thisHash);

        inFile.close();
        m_processedCount++;
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

string DDOperationRebuild::GetOperationSummation()
{
    string summation;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(m_endProcessing - m_startProcessing).count();
    double readSpeed = (elapsedSeconds > 0) ? (m_processedSize / elapsedSeconds) : 0.0;
    summation = std::format(std::locale(""), "{:L} items processed. {:L} bytes read in {:.6Lf} seconds ({:.2Lf} bytes per second)",
                            m_processedCount.load(), m_processedSize.load(), elapsedSeconds, readSpeed);
    return summation;
}

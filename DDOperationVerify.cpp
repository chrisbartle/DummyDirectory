#include "DDOperationVerify.h"
#include "DDMD5Hasher.h"

#include <fstream>

void DDOperationVerify::ChildDoOperation(DDParameters &parameters)
{
    //Set expectations
    m_targetSize = m_manifest.getTotalSize();
    m_targetCount = m_manifest.getTotalFileCount();

    //Iterate through every file in the manifest
    for (uint64_t filePos = 0; (filePos < m_manifest.getTotalFileCount()); filePos++)
    {
        DDFile& file = m_manifest.getFileByPos(filePos);
        file.setProcessingStatus(DDFile::QUEUED);
        DoFileOperation(file, parameters, 0, 0);
    }
}

void DDOperationVerify::ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size)
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
        }
        //The file sizes must match
        else if (std::filesystem::file_size(absolutePathname) != file.size())
            file.setProcessingStatus(DDFile::DIFFERENT);
        else
        {
            //Open the file
            std::ifstream inFile(absolutePathname, std::ios::binary);

            //Iterate through the file data and build the hash
            DDMD5Hasher hasher;
            char buffer[BUFFER_SIZE];
            inFile.read(buffer, BUFFER_SIZE);
            while (inFile.gcount() > 0)
            {
                hasher.update(buffer, inFile.gcount());
                m_processedSize += inFile.gcount();
                inFile.read(buffer, BUFFER_SIZE);
            }

            //Finalize the hash
            std::string thisHash = hasher.finalize();

            if (thisHash != file.hash())
                //The hashes are different
                file.setProcessingStatus(DDFile::DIFFERENT);
            else
                file.setProcessingStatus(DDFile::COMPLETE);

            inFile.close();
        }
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
}

string DDOperationVerify::GetOperationSummation()
{
    string summation;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(m_endProcessing - m_startProcessing).count();
    double readSpeed = (elapsedSeconds > 0) ? (m_processedSize / elapsedSeconds) : 0.0;
    summation = std::format(std::locale(""), "{:L} items processed. {:L} bytes read in {:.6Lf} seconds ({:.2Lf} bytes per second)",
                            m_processedCount.load(), m_processedSize.load(), elapsedSeconds, readSpeed);
    return summation;
}

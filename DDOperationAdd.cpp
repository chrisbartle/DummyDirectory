#include "DDOperationAdd.h"
#include "DDDeterministicPCGPRNG.h"
#include "DDMD5Hasher.h"

#include <fstream>

void DDOperationAdd::ChildSetDefaultParameters(DDParameters &parameters)
{
    //Either the size or the count must be filled in
    if (!parameters.isFlag("size") && !parameters.isFlag("count"))
    {
        //100 megabytes is a good default
        parameters.setFlag("size", "100M");
    }
    //There should always be a limit on the individual file size or one enormous file is the likely result
    if (!parameters.isFlag("filesize"))
    {
        //Any individual file will be between 1 kilobyte and 10 megabyte
        parameters.setFlag("filesize", "1k-10m");
    }
}

void DDOperationAdd::ChildDoOperation(DDParameters &parameters)
{
    //We need to determine our target point. It may either be size (the total number of bytes to be processed)
    //or count (the total number of objects to be created
    m_targetSize = 0;
    m_targetCount = 0;
    if (parameters.isFlag("size"))
        m_targetSize = m_rng.processFlag(parameters.getFlag("size"), m_manifest.getTotalSize());
    if (parameters.isFlag("count"))
        m_targetCount = m_rng.processFlag(parameters.getFlag("count"), m_manifest.getTotalFileCount());

    uint64_t sizeSoFar = 0;
    uint64_t countSoFar = 0;
    //Loop until either the target size or target count is reached
    while ((parameters.isFlag("size") && (sizeSoFar < m_targetSize))
           || (parameters.isFlag("count") && (countSoFar < m_targetCount)))
    {
        //Is this going to be binary or text? Binary files end in .bin and text files end in .txt
        std::string fileExtension;
        if (!parameters.isFlag("filetype") || (parameters.getFlag("filetype") == "random"))
        {
            //Pick the filetype at random
            uint64_t randomFileType = m_rng.getFromRange(0, 2);
            if (randomFileType == 1)
                fileExtension = ".txt";
            else if (randomFileType == 2)
                fileExtension = ".sprs";
            else
                fileExtension = ".bin";
        }
        else if (parameters.getFlag("filetype") == "text")
            fileExtension = ".txt";
        else if (parameters.getFlag("filetype") == "sparse")
            fileExtension = ".sprs";
        else
            fileExtension = ".bin";

        //Pick a directory where this file will be added
        uint64_t dirPos = m_rng.getFromRange(0, m_manifest.getTotalDirectoryCount()-1);
        DDDirectory directory = m_manifest.getDirectoryByPos(dirPos);
        //Add the new file to manifest and make up a filename
        string filename = "DD_" + m_rng.getSimpleString(10) + fileExtension;
        DDFile& file = m_manifest.addFile();
        file.setRelativePathname(directory.relativePath() / filename);
        //Come up with the file's size
        uint64_t fileSize = m_rng.processFlag(parameters.getFlag("filesize"), m_manifest.getTotalSize());
        if (fileSize < MINIMUM_FILE_SIZE)
            fileSize = MINIMUM_FILE_SIZE;
        uint64_t fileSeed = m_rng.get64();
        if (parameters.isFlag("fileseed"))
            fileSeed = m_rng.processFlag(parameters.getFlag("fileseed"));
        //See if there's a total size limit to enforce here
        if (parameters.isFlag("size"))
        {
            //If the file is too big to fit inside the target total, decrease it
            if (fileSize + sizeSoFar > m_targetSize)
                fileSize = m_targetSize - sizeSoFar;
            //Files must be at least 15 bytes in size in order to hold the header information
            if (fileSize < MINIMUM_FILE_SIZE)
                fileSize = MINIMUM_FILE_SIZE;
        }
        file.setProcessingStatus(DDFile::QUEUED);

        DoFileOperation(file, parameters, fileSeed, fileSize);

        //Increment
        sizeSoFar += fileSize;
        countSoFar += 1;
    }
}

void DDOperationAdd::ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size)
{
    //We can only processed queued items
    if (file.processingStatus() != DDFile::QUEUED)
        return;

    filesystem::path absolutePathname = parameters.ConvertToAbsolutePath(file.relativePathname());

    //Mark that this file is being processed
    file.setProcessingStatus(DDFile::STARTED);
    try
    {
        DDDeterministicPCGPRNG rng(seed);

        //We don't want to overwrite an existing file
        if (std::filesystem::exists(absolutePathname))
        {
            file.setProcessingStatus(DDFile::CONFLICT);
            return;
        }

        std::ios::openmode ofstreamFlags = std::ios::out;
        bool isText = false;
        if (file.relativePathname().extension() == ".txt")
            isText = true;
        //        else
        //Looks like we want the binary flag set as not setting it creates OS dependant
        //rules on how to handle text.
        ofstreamFlags |= std::ios::binary;

        //Open filestream
        std::ofstream outFile(absolutePathname, ofstreamFlags);

        uint64_t writtenSoFar = 0;
        DDMD5Hasher hasher;

        //A new file always starts with the prefix
        string prefix = m_filePrefix;
        //Binary files follow with a null, text files with a space
        if (isText)
            prefix += ' ';
        else
            prefix += '\0';
        outFile.write(prefix.data(), prefix.length());
        hasher.update(prefix.data(), prefix.length());
        writtenSoFar += prefix.length();
        m_processedSize += prefix.length();

        if (isText)
        {
            //Text file. Write out randomly generated words until the exact size is reached
            while (writtenSoFar < size)
            {
                uint64_t bufferSize = BUFFER_SIZE;
                if (writtenSoFar + bufferSize > size)
                    bufferSize = size-writtenSoFar;
                string textBuffer = rng.getText(bufferSize);
                outFile.write(textBuffer.data(), bufferSize);
                hasher.update(textBuffer.data(), bufferSize);
                writtenSoFar += bufferSize;
                m_processedSize += bufferSize;
            }
        }
        else
        {
            //Binary file. Write random bytes out until the exact size is reached.
            vector<uint8_t> buffer(BUFFER_SIZE);
            if (file.relativePathname().extension() == ".sprs")
            {
                //A sparse file is a binary file that has lot of empty space. In order to create it,
                //we start writing at an arbitrary point more than halfway into the file.
                uint64_t halfwayPoint = size/2;
                if (halfwayPoint < MINIMUM_FILE_SIZE)
                    halfwayPoint = MINIMUM_FILE_SIZE;
                uint64_t startPoint = rng.getFromRange(halfwayPoint, size-1);
                //Run the md5 hasher on null data
                while (writtenSoFar < startPoint)
                {
                    uint64_t bufferSize = BUFFER_SIZE;
                    if (writtenSoFar + bufferSize > startPoint)
                        bufferSize = startPoint-writtenSoFar;
                    hasher.update(reinterpret_cast<const char*>(buffer.data()), bufferSize);
                    writtenSoFar += bufferSize;
                    m_processedSize += bufferSize;
                }
                outFile.seekp(startPoint);
            }
            while (writtenSoFar < size)
            {
                uint64_t bufferSize = BUFFER_SIZE;
                if (writtenSoFar + bufferSize > size)
                    bufferSize = size-writtenSoFar;
                buffer = rng.getBytes(bufferSize);
                outFile.write(reinterpret_cast<const char*>(buffer.data()), bufferSize);
                hasher.update(reinterpret_cast<const char*>(buffer.data()), bufferSize);
                writtenSoFar += bufferSize;
                m_processedSize += bufferSize;
            }
        }

        outFile.close();
        //Update the file stats
        file.setSize(size);
        file.setHash(hasher.finalize());
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

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
            if (m_rng.getFromRange(0, 1) == 1)
                fileExtension = ".txt";
            else
                fileExtension = ".bin";
        }
        else if (parameters.getFlag("filetype") == "text")
            fileExtension = ".txt";
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
        //See if there's a total size limit to enforce here
        if (parameters.isFlag("size"))
        {
            //If the file is too big to fit inside the target total, decrease it
            if (fileSize + sizeSoFar > m_targetSize)
                fileSize = m_targetSize - sizeSoFar;
            //Files must be at least 15 bytes in size in order to hold the header information
            if (fileSize < 15)
                fileSize = 15;
        }
        file.setProcessingStatus(DDFile::QUEUED);

        DoFileOperation(file, parameters, m_rng.get64(), fileSize);

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
        string operation = parameters.getOperation();
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

        //If this is a text file, generate a dictionary of random words
        //between 20 and 1000 words
        //each word between 1 and 20 characters
        vector<string> textDictionary;
        if (isText)
        {
            int dictionarySize = rng.getFromRange(20, 100);
            textDictionary.reserve(dictionarySize);
            for(int rut = 0; (rut < dictionarySize); rut++)
                textDictionary.push_back(rng.getSimpleString(rng.getFromRange(1, 20)));
            //Put in the occasional carriage return
            textDictionary.push_back("\n");
        }

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
            //Text file. Write out words from the dictionary until the exact size is reached
            string textBuffer;
            textBuffer.reserve(BUFFER_SIZE);
            while (writtenSoFar < size)
            {
                textBuffer.clear();
                uint64_t bufferSize = BUFFER_SIZE;
                if (writtenSoFar + bufferSize > size)
                    bufferSize = size-writtenSoFar;
                while (textBuffer.length() < bufferSize)
                    textBuffer += textDictionary[rng.getFromRange(0, textDictionary.size()-1)] + " ";
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

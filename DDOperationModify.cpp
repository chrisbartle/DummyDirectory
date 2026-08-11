#include "DDOperationModify.h"
#include "DDDeterministicPCGPRNG.h"
#include "DDMD5Hasher.h"

#include <numeric>
#include <fstream>

DDOperationModify::ModificationType DDOperationModify::ConvertStringToModifcationType(string inModificationType)
{
    if (inModificationType == "append")
        return APPEND;
    else if (inModificationType == "truncate")
        return TRUNCATE;
    else if (inModificationType == "overwrite")
        return OVERWRITE;
    else if (inModificationType == "chop")
        return CHOP;
    else if (inModificationType == "insert")
        return INSERT;
    throw runtime_error(inModificationType + " is not a valid modification type");
}

void DDOperationModify::ChildSetDefaultParameters(DDParameters &parameters)
{
    //Either the size or the count must be filled in
    if (!parameters.isFlag("size") && !parameters.isFlag("count"))
    {
        //Our default will be to modify 10% of the files
        parameters.setFlag("count", "10%");
    }

    //In the context of the modify operation, filesize determines how much the fill will change
    //We'll set the default to between 5% and 100%
    if (!parameters.isFlag("filesize"))
    {
        parameters.setFlag("filesize", "5%-100%");
    }
}

void DDOperationModify::ChildDoOperation(DDParameters &parameters)
{
    uint64_t totalFileCount = m_manifest.getTotalFileCount();
    if (totalFileCount == 0)
        return;

    //We need to determine our target point. It may either be size (the total number of bytes to be delete)
    //or count (the total number of objects to be deleted)
    m_targetSize = 0;
    m_targetCount = 0;
    m_processedFileSizeTotal = 0;
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
    //that we efficiently modify the correct number of files even if the user requests a high percentage (90%)
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
            uint64_t fileChangeSize = m_rng.processFlag(parameters.getFlag("filesize"), file.size());
            if (parameters.isFlag("size") && (sizeSoFar+fileChangeSize > m_targetSize))
                fileChangeSize = m_targetSize-sizeSoFar;
            sizeSoFar += fileChangeSize;
            countSoFar++;
            uint64_t fileSeed = m_rng.get64();
            if (parameters.isFlag("fileseed"))
                fileSeed = m_rng.processFlag(parameters.getFlag("fileseed"));
            file.setProcessingStatus(DDFile::QUEUED);
            DoFileOperation(file, parameters, fileSeed, fileChangeSize);
        }
        // Jump forward by the stride and wrap around using modulo
        filePos = (filePos + stride) % totalFileCount;
        iteratorCounter++;
        if (iteratorCounter > totalFileCount)
            //Maybe user is trying to modify 110%?
            break;
    }
}

void DDOperationModify::ChildDoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size)
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
        DDMD5Hasher hasher;

        //Determine the modification type. Use what's been defined or pick one at random
        ModificationType modifyType;
        if (parameters.isFlag("modifytype"))
        {
            string modifyTypeStr = parameters.getFlag("modifytype");
            if (modifyTypeStr == "random")
                modifyType = ModificationType(rng.getFromRange(0, ModificationTypeCount-1));
            else
                modifyType = ConvertStringToModifcationType(modifyTypeStr);
        }
        else
            modifyType = ModificationType(rng.getFromRange(0, ModificationTypeCount-1));
        string fileExtension = file.relativePathname().extension().string();

        //Make sure the file is there
        if (!std::filesystem::exists(absolutePathname))
        {
            file.setProcessingStatus(DDFile::MISSING);
            return;
        }

        //The easiest way to truncate a file is with a call to resize_file
        if (modifyType == TRUNCATE)
        {
            uint64_t newFileSize = file.size() - size;
            if (newFileSize < MINIMUM_FILE_SIZE)
                newFileSize = MINIMUM_FILE_SIZE;
            filesystem::resize_file(absolutePathname, newFileSize);
            file.setSize(newFileSize);
        }

        //Open filestream. We need to read from it to build the hash even if we plan to change parts of it
        fstream existingFile(absolutePathname, std::ios::in | std::ios::out | std::ios::binary);

        if (modifyType == TRUNCATE)
        {
            //With truncate, we've already done the truncation. We just need to build a hash of the new, truncated file.
            readFile(existingFile, hasher, file.size());
        }
        else if (modifyType == APPEND)
        {
            //To append, we build a hash of the existing file and then write more to the end
            readFile(existingFile, hasher, file.size());
            existingFile.clear();
            existingFile.seekp(existingFile.tellg());
            writeFile(existingFile, fileExtension, hasher, rng, size);
            uint64_t newFileSize = file.size() + size;
            file.setSize(newFileSize);
        }
        else if (modifyType == OVERWRITE)
        {
            //Pick a random start and end point in the file, then overwrite it
            //The start point needs to be greater than the minimum but still provide room to write
            //The file size should not change
            int prefixSize = m_filePrefix.length();
            if (size > file.size() - prefixSize)
                //We never want to overwrite the prefix, even if the change is 100% of the file size
                size = file.size() - prefixSize;
            uint64_t startPos = rng.getFromRange(prefixSize, file.size()-size);
            readFile(existingFile, hasher, startPos);
            existingFile.clear();
            existingFile.seekp(existingFile.tellg());
            writeFile(existingFile, fileExtension, hasher, rng, size);
            existingFile.flush();
            existingFile.clear();
            existingFile.seekg(0, std::ios::cur);
            uint64_t endPos = startPos + size;
            readFile(existingFile, hasher, file.size() - endPos);
        }
        else if (modifyType == CHOP)
        {
            //Pick a random start and end point in the file and then remove that data.
            //The most efficient way to do this is to write parts out to a new file
            //and copy it over the original
            string tempFilename = "DD_" + m_rng.getSimpleString(10) + ".tmp";
            filesystem::path absoluteTempPathname = absolutePathname;
            absoluteTempPathname.replace_filename(tempFilename);
            fstream tempFile(absoluteTempPathname, std::ios::out | std::ios::binary );//| std::ios::trunc);
            if (size > file.size()-MINIMUM_FILE_SIZE)
                size = file.size()-MINIMUM_FILE_SIZE;
            uint64_t startPos = rng.getFromRange(MINIMUM_FILE_SIZE, file.size()-size);
            copyFile(existingFile, tempFile, hasher, startPos);
            uint64_t endPos = startPos + size;
            existingFile.seekg(endPos);
            copyFile(existingFile, tempFile, hasher, file.size() - endPos);
            existingFile.close();
            tempFile.close();
            filesystem::remove(absolutePathname);
            filesystem::rename(absoluteTempPathname, absolutePathname);
            uint64_t newFileSize = file.size() - size;
            file.setSize(newFileSize);
        }
        else if (modifyType == INSERT)
        {
            //Pick a random point in the file and insert data.
            //The most efficient way to do this is to write out to a new file and copy it back.
            string tempFilename = "DD_" + m_rng.getSimpleString(10) + ".tmp";
            filesystem::path absoluteTempPathname = absolutePathname;
            absoluteTempPathname.replace_filename(tempFilename);
            fstream tempFile(absoluteTempPathname, std::ios::out | std::ios::binary );//| std::ios::trunc);
            uint64_t insertPos = rng.getFromRange(MINIMUM_FILE_SIZE, file.size());
            copyFile(existingFile, tempFile, hasher, insertPos);
            writeFile(tempFile, fileExtension, hasher, rng, size);
            copyFile(existingFile, tempFile, hasher, file.size()-insertPos);
            existingFile.close();
            tempFile.close();
            filesystem::remove(absolutePathname);
            filesystem::rename(absoluteTempPathname, absolutePathname);
            uint64_t newFileSize = file.size() + size;
            file.setSize(newFileSize);
        }
        else
            throw runtime_error("Unkown modification type!");

        //Update the file stats
        file.setHash(hasher.finalize());
        if (existingFile.is_open())
            existingFile.close();
        m_processedCount++;
        m_processedSize += size;
        m_processedFileSizeTotal += file.size();
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

string DDOperationModify::GetOperationSummation()
{
    string summation;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(m_endProcessing - m_startProcessing).count();
    double writeSpeed = (elapsedSeconds > 0) ? (m_processedSize / elapsedSeconds) : 0.0;
    summation = std::format(std::locale(""), "{:L} items processed. {:L} bytes worth of changes in {:.6Lf} seconds ({:.2Lf} bytes per second)\nThe total size of all affected files is {:L} bytes",
                            m_processedCount.load(), m_processedSize.load(), elapsedSeconds, writeSpeed, m_processedFileSizeTotal);
    return summation;
}

void DDOperationModify::readFile(fstream &inFile, DDMD5Hasher &hasher, uint64_t size)
{
    char buffer[BUFFER_SIZE];
    uint64_t totalBytesRead = 0;
    while ((totalBytesRead < size) && !inFile.eof())
    {
        uint64_t bufferSize = BUFFER_SIZE;
        if (totalBytesRead + bufferSize > size)
            bufferSize = size-totalBytesRead;
        inFile.read(buffer, bufferSize);
        hasher.update(buffer, inFile.gcount());
        totalBytesRead += inFile.gcount();
    }
    //Set up the file for writing where this reading left off
//    inFile.clear();
//    inFile.seekp(std::ios::cur);
}

void DDOperationModify::writeFile(fstream &inFile, string fileExtension, DDMD5Hasher &hasher, DDDeterministicPCGPRNG &inRNG, uint64_t size)
{
    uint64_t writtenSoFar = 0;

    if (fileExtension == ".txt")
    {
        //Text file. Write out randomly generated words until the exact size is reached
        while (writtenSoFar < size)
        {
            uint64_t bufferSize = BUFFER_SIZE;
            if (writtenSoFar + bufferSize > size)
                bufferSize = size-writtenSoFar;
            string textBuffer = inRNG.getText(bufferSize);
            inFile.write(textBuffer.data(), bufferSize);
            hasher.update(textBuffer.data(), bufferSize);
            writtenSoFar += bufferSize;
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
            buffer = inRNG.getBytes(bufferSize);
            inFile.write(reinterpret_cast<const char*>(buffer.data()), bufferSize);
            hasher.update(reinterpret_cast<const char*>(buffer.data()), bufferSize);
            writtenSoFar += bufferSize;
        }
    }
    //Set up this file for reading where the writing left off
//    inFile.flush();
//    inFile.clear();
//    inFile.seekg(0, std::ios::cur);
}

void DDOperationModify::copyFile(fstream &sourceFile, fstream &destinationFile, DDMD5Hasher &hasher, uint64_t size)
{
    uint64_t copiedSoFar = 0;
    char buffer[BUFFER_SIZE];
    while ((copiedSoFar < size) && !sourceFile.eof())
    {
        uint64_t bufferSize = BUFFER_SIZE;
        if (copiedSoFar + bufferSize > size)
            bufferSize = size-copiedSoFar;
        sourceFile.read(buffer, bufferSize);
        destinationFile.write(buffer, sourceFile.gcount());
        hasher.update(buffer, sourceFile.gcount());
        copiedSoFar += sourceFile.gcount();
    }
}

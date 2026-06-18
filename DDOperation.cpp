#include "DDOperation.h"
#include "DDDeterministicPCGPRNG.h"
#include "BS_thread_pool.hpp"
#include "DDMD5Hasher.h"

#include <random>
#include <fstream>

using namespace std;

DDOperation::DDOperation(DDManifest &inManifest) : m_manifest(inManifest)
{
    m_processedSize = 0;
    m_processedCount = 0;

    //Generate the file prefix that is used to generating new files
    m_filePrefix = DDFile::getApplicationIDString();
}

/**
 * @brief DDOperation::SetDefaultParameters
 * DoOperation expects all necessary parameters to be filled in. This function will
 * add any necessary defaults so that DoOperation can run appropriately
 * @param parameters The DDParameters object that will be used for the operation
 */
void DDOperation::SetDefaultParameters(DDParameters &parameters)
{
    //The seed must always be filled in
    if (!parameters.isFlag("seed"))
    {
        //A predefined seed value is not being provided, we'll generate our own
        //and store it in the parameters so that it gets logged properly.
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 engine(rd());
        static std::uniform_int_distribution<uint64_t> dist; // Default range is min() to max()
        string newSeed = format("{:x}", dist(engine));
        parameters.setFlag("seed", newSeed);
    }

    string operation = parameters.getOperation();
    if (operation == "add")
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
    else if (operation == "delete")
    {
        //file deletion can be restricted by size or count
        if (!parameters.isFlag("size") && !parameters.isFlag("count"))
        {
            //Delete 20% of files
            parameters.setFlag("count", "20%");
        }
    }
    else if (operation == "modify")
    {
        //file modification can be restricted by size or count
        if (!parameters.isFlag("size") && !parameters.isFlag("count"))
        {
            //Modify 20% of files
            parameters.setFlag("count", "20%");
        }
    }
    else if (operation == "delete")
    {

    }
    else if (operation == "delete")
    {

    }
    else if (operation == "delete")
    {

    }
    else if (operation == "delete")
    {

    }
    else if (operation == "delete")
    {

    }
}

/**
 * @brief DDOperation::DoOperation
 * Perform an operation in the dummy directory
 * @param parameters The parameters to apply against this operation
 */
void DDOperation::DoOperation(DDParameters &parameters)
{
    //Make sure that the operation is valid
    if (ValidateOperationType(parameters.getOperation()).length() > 0)
        throw std::runtime_error("Operation type " + parameters.getOperation() + " is not known");

    //Reset the counts
    m_processedSize = 0;
    m_processedCount = 0;

    //Initialize the deterministic random number generator
    DDDeterministicPCGPRNG rng;
    //The parameter contains the seed value stored as a hex string
    rng.Seed(stoull(parameters.getFlag("seed"), nullptr, 16));

    string operation = parameters.getOperation();
    //See if these are file based operations
    if ((operation == "add")                      //add files
        || (operation == "delete")            //delete files
        || (operation == "modify")            //modify files
        || (operation == "rename")            //rename files
        || (operation == "move")              //move files to a different directory
        || (operation == "clean")             //delete all files and directories
        || (operation == "verify")            //confirm that the manifest is correct
        || (operation == "rebuild")
        )
    {
        //For add file operations, the first thing we need to do is initialize the thread pool
        BS::thread_pool threadPool;
        //See if the threads are set by parameter
        if (parameters.isFlag("threads"))
            threadPool.reset(stoul(parameters.getFlag("threads")));

        //We need to determine our target point. It may either be size (the total number of bytes to be processed)
        //or count (the total number of objects to be created
        uint64_t targetSize = 0;
        uint64_t targetCount = 0;
        if (parameters.isFlag("size"))
            targetSize = rng.processFlag(parameters.getFlag("size"), m_manifest.getTotalSize());
        if (parameters.isFlag("count"))
            targetCount = rng.processFlag(parameters.getFlag("count"), m_manifest.getTotalFileCount());

        if (operation == "add")
        {
            uint64_t sizeSoFar = 0;
            uint64_t countSoFar = 0;
            //Loop until either the target size or target count is reached
            while ((parameters.isFlag("size") && (sizeSoFar < targetSize))
                   || (parameters.isFlag("count") && (countSoFar < targetCount)))
            {
                //Is this going to be binary or text? Binary files end in .bin and text files end in .txt
                std::string fileExtension;
                if (!parameters.isFlag("filetype") || (parameters.getFlag("filetype") == "random"))
                {
                    //Pick the filetype at random
                    if (rng.getFromRange(0, 1) == 1)
                        fileExtension = ".txt";
                    else
                        fileExtension = ".bin";
                }
                else if (parameters.getFlag("filetype") == "text")
                    fileExtension = ".txt";
                else
                    fileExtension = ".bin";

                //Pick a directory where this file will be added
                uint64_t dirPos = rng.getFromRange(0, m_manifest.getTotalDirectoryCount()-1);
                DDDirectory directory = m_manifest.getDirectoryByPos(dirPos);
                //Add the new file to manifest and make up a filename
                string filename = "DD_" + rng.getSimpleString(10) + fileExtension;
                DDFile& file = m_manifest.addFile();
                file.setRelativePathname(directory.relativePath() / filename);
                //Come up with the file's size
                uint64_t fileSize = rng.processFlag(parameters.getFlag("filesize"), m_manifest.getTotalSize());
                //If the file is too big to fit inside the target total, decrease it
                if (fileSize + sizeSoFar > targetSize)
                    fileSize = targetSize - sizeSoFar;
                //Files must be at least 15 bytes in size in order to hold the header information
                if (fileSize < 15)
                    fileSize = 15;
                file.setProcessingStatus(DDFile::QUEUED);

                //User can force everything to run in a single thread by setting threads to 0 or 1
                if (parameters.isFlag("threads") && stoul(parameters.getFlag("threads")) < 2)
                    DoFileOperation(file, parameters, rng.get64(), fileSize);
                else
                {
                    //Hand it to the thread pool for processing
                    threadPool.detach_task(
                        [&file, &parameters, &rng, fileSize, this]
                        {
                            DoFileOperation(file, parameters, rng.get64(), fileSize);
                        });
                }

                //Increment
                sizeSoFar += fileSize;
                countSoFar += 1;
            }

            //Wait for the thread pool to complete
            threadPool.wait();
        }
    }
}

void DDOperation::DoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size)
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
        if ((operation == "add") && std::filesystem::exists(absolutePathname))
        {
            file.recordProcessingError("A file already exists at " + absolutePathname.string());
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
            int dictionarySize = rng.getFromRange(20, 1000);
            textDictionary.reserve(dictionarySize);
            for(int rut = 0; (rut < dictionarySize); rut++)
                textDictionary.push_back(rng.getSimpleString(rng.getFromRange(1, 20)));
        }

        uint64_t writtenSoFar = 0;
        DDMD5Hasher hasher;
        while (writtenSoFar < size)
        {
            //A new file always starts with the prefix
            if (writtenSoFar == 0)
            {
                string prefix = m_filePrefix;
                //Binary files follow with a null, text files with a space
                if (isText)
                    m_filePrefix += ' ';
                else
                    m_filePrefix += '\0';
                outFile.write(prefix.data(), prefix.length());
                writtenSoFar += prefix.length();
                hasher.update(prefix.data(), prefix.length());
                continue;
            }

            uint64_t bufferSize = BUFFER_SIZE;
            if (writtenSoFar + bufferSize > size)
                bufferSize = size-writtenSoFar;
            vector<uint8_t> buffer(bufferSize);
            if (isText)
            {
                string textBuffer;
                textBuffer.resize(bufferSize);
                while (textBuffer.length() < bufferSize)
                    textBuffer += textDictionary[rng.getFromRange(0, textDictionary.size())] + " ";
                outFile.write(textBuffer.data(), bufferSize);
                hasher.update(textBuffer.data(), bufferSize);
            }
            else
            {
                buffer = rng.getBytes(bufferSize);
                outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
                hasher.update(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            }
            writtenSoFar += bufferSize;
        }
        outFile.close();
        //Update the file stats
        file.setSize(size);
        file.setHash(hasher.finalize());
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

/**
 * @brief DDOperation::ValidateOperationType
 * Given the name of an operation (add, delete, modify, etc), confirms that it is a valid
 * and recognizable type.
 * @param inOperation The type of operation to be run
 * @return true if the operation is a valid type
 */
std::string DDOperation::ValidateOperationType(std::string inOperation)
{
    if ((inOperation == "add")                      //add files
            || (inOperation == "delete")            //delete files
            || (inOperation == "modify")            //modify files
            || (inOperation == "rename")            //rename files
            || (inOperation == "move")              //move files to a different directory
            || (inOperation == "add_directory")     //add directories
            || (inOperation == "rename_directory")  //rename directories
            || (inOperation == "move_directory")    //move directories to a different directory
            || (inOperation == "delete_directory")  //delete directories
            || (inOperation == "clean")             //delete all files and directories
            || (inOperation == "verify")            //confirm that the manifest is correct
            || (inOperation == "rebuild")           //build a new manifest from the contents of this directory
            )
        return "";
    return "Unknown operation " + inOperation;
}

string DDOperation::ValidateFlag(std::string inOperation, std::string inFlag, std::string inFlagValue)
{
    //Not possible to really validate these
    if ((inFlag == "replay")
            || (inFlag == "verbose")
        )
        return "";

    if (inFlag == "threads")
    {
        try
        {
            (void)stoul(inFlagValue);
        }
        catch(...)
        {
            return "The following number of threads value can not be parsed: " + inFlagValue;
        }
    }


    if (inFlag == "seed")
    {
        //The seed should be a hex number. Try to convert it.
        try
        {
            (void)stoull(inFlagValue, nullptr, 16);
        }
        catch(...)
        {
            return "The following seed value can not be parsed: " + inFlagValue;
        }
    }

    //These follow the numeric rules. The easiest approach is to hand them to be parsed
    //and report if they throw an exception.
    if ((inFlag == "size")  //Total size of operation, in bytes
        || (inFlag == "count")  //Total number of objects to by manipulated during operation
        || (inFlag == "filesize")   //Total number of bytes to be processed per file
        )
    {
        try {
            DDDeterministicPCGPRNG tempRNG;
            //These operations all for a percentage to be applied
            uint64_t percentageAllowed = 0;
            if ((inOperation == "delete")            //delete files
                || (inOperation == "modify")            //modify files
                || (inOperation == "rename")            //rename files
                || (inOperation == "move")              //move files to a different directory
                || (inOperation == "rename_directory")  //rename directories
                || (inOperation == "move_directory")    //move directories to a different directory
                || (inOperation == "delete_directory")  //delete directories
                )
                percentageAllowed = 1000000;
            tempRNG.convertStringToNumber(inFlagValue, percentageAllowed);
        } catch (...) {
            return "Can not parse flag " + inFlag;
        }
    }

    if (inFlag == "filetype")
    {
        if ((inFlagValue != "random") || (inFlagValue != "binary") && (inFlagValue != "text"))
            return "file_type must be set to random, binary, or text";
        if (inOperation != "add")
            return "file_type can only be set for add operations";
    }

    if (inFlag == "modifytype")
    {
        if ((inFlagValue != "random") || (inFlagValue != "overwrite") && (inFlagValue != "block") && (inFlagValue != "append"))
            return "file_type must be set to random, overwrite, block, or append";
        if (inOperation != "modify")
            return "file_type can only be set for modify operations";
    }

    return "";
}

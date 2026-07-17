#include "DDOperation.h"
#include "DDDeterministicPCGPRNG.h"
#include "BS_thread_pool.hpp"

#include "DDOperationAdd.h"
#include "DDOperationModify.h"
#include "DDOperationRename.h"
#include "DDOperationMove.h"
#include "DDOperationDelete.h"
#include "DDOperationAddDirectory.h"
#include "DDOperationRenameDirectory.h"
#include "DDOperationMoveDirectory.h"
#include "DDOperationDeleteDirectory.h"
#include "DDOperationVerify.h"
#include "DDOperationClean.h"
#include "DDOperationRebuild.h"

#include <assert.h>
#include <random>
#include <format>
#include <locale>

using namespace std;

DDOperation::DDOperation(DDManifest &inManifest) : m_manifest(inManifest)
{
    m_processedSize = 0;
    m_processedCount = 0;
    m_targetSize = 0;
    m_targetCount = 0;

    //Generate the file prefix that is used to generating new files
    m_filePrefix = "DummyDir_v1";
    assert(m_filePrefix.length() <= MINIMUM_FILE_SIZE);
}

/**
 * @brief DDOperation::getOperationByName
 * When passed the name of the operation, returns an appropriate operation object. For example, "add"
 * returns DDOperationAdd.
 * @param inOperation The name of the operation as passed by parameter
 * @param inManifest The manifest file
 * @return The appropriate DDOperation child object
 */
std::unique_ptr<DDOperation> DDOperation::getOperationByName(std::string inOperation, DDManifest &inManifest)
{
    if (inOperation == "add")
        return std::make_unique<DDOperationAdd>(inManifest);
    else if (inOperation == "modify")
        return std::make_unique<DDOperationModify>(inManifest);
    else if (inOperation == "rename")
        return std::make_unique<DDOperationRename>(inManifest);
    else if (inOperation == "move")
        return std::make_unique<DDOperationMove>(inManifest);
    else if (inOperation == "delete")
        return std::make_unique<DDOperationDelete>(inManifest);
    else if (inOperation == "dadd")
        return std::make_unique<DDOperationAddDirectory>(inManifest);
    else if (inOperation == "drename")
        return std::make_unique<DDOperationRenameDirectory>(inManifest);
    else if (inOperation == "dmove")
        return std::make_unique<DDOperationMoveDirectory>(inManifest);
    else if (inOperation == "ddelete")
        return std::make_unique<DDOperationDeleteDirectory>(inManifest);
    else if (inOperation == "verify")
        return std::make_unique<DDOperationVerify>(inManifest);
    else if (inOperation == "clean")
        return std::make_unique<DDOperationClean>(inManifest);
    else if (inOperation == "rebuild")
        return std::make_unique<DDOperationRebuild>(inManifest);
    throw runtime_error("Unknown operation " + inOperation);
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

    ChildSetDefaultParameters(parameters);
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
    m_targetSize = 0;
    m_targetCount = 0;

    //Reseed the random number generator
    //The parameter contains the seed value stored as a hex string
    m_rng.Seed(stoull(parameters.getFlag("seed"), nullptr, 16));

    //Set the timer
    m_startProcessing = std::chrono::steady_clock::now();

    //Have the child class do the actual operation
    ChildDoOperation(parameters);

    //Wait for the thread pool to complete
    WaitForThreadsToComplete();

    //Stop the timer
    m_endProcessing = std::chrono::steady_clock::now();
}

string DDOperation::GetOperationSummation()
{
    string summation;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(m_endProcessing - m_startProcessing).count();
    double writeSpeed = (elapsedSeconds > 0) ? (m_processedSize / elapsedSeconds) : 0.0;
    summation = std::format(std::locale(""), "{:L} items processed. {:L} bytes written in {:.6Lf} seconds ({:.2Lf} bytes per second)",
                            m_processedCount.load(), m_processedSize.load(), elapsedSeconds, writeSpeed);
    return summation;
}

void DDOperation::DoFileOperation(DDFile &file, DDParameters &parameters, uint64_t seed, uint64_t size)
{
    //If user has set the number of threads to 0 or 1 then just run the file operation in this thread
    if (parameters.isFlag("threads") && stoul(parameters.getFlag("threads")) < 2)
        ChildDoFileOperation(file, parameters, seed, size);

    //Has the thread pool been initialized? We initialize it only when needed since it will create
    //multiple threads.
    if (m_threadPool == NULL)
    {
        //The number of threads can be passed in as a parameter
        if (parameters.isFlag("threads"))
        {
            uint64_t num_threads = stoul(parameters.getFlag("threads"));
            m_threadPool = std::make_unique<BS::light_thread_pool>(num_threads);
        }
        else
            m_threadPool = std::make_unique<BS::light_thread_pool>();
    }

    //Create a job and pass to the thread pool
    m_threadPool->detach_task(
        [&file, &parameters, seed, size, this]
        {
            ChildDoFileOperation(file, parameters, seed, size);
        });
}

void DDOperation::WaitForThreadsToComplete()
{
    if (m_threadPool)
        while(!m_threadPool->wait_for(std::chrono::milliseconds(500)))
        {
            //Still processing, issue the callback
            UpdateProcessingStatus();
        }
}

/**
 * @brief DDOperation::UpdateProcessingStatus
 * Calls the callback function and provides information on the percentage completed
 */
void DDOperation::UpdateProcessingStatus()
{
    //If there is a callback function, calculate the percentage and call it
    if (m_statusCallbackFunction)
    {
        double percentage = 1;
        if (m_targetSize > 0)
            percentage = static_cast<double>(m_processedSize)/m_targetSize;
        else if (m_targetCount > 0)
            percentage = static_cast<double>(m_processedCount)/m_targetCount;
        m_statusCallbackFunction(percentage);
    }
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
            || (inOperation == "dadd")     //add directories
            || (inOperation == "drename")  //rename directories
            || (inOperation == "dmove")    //move directories to a different directory
            || (inOperation == "ddelete")  //delete directories
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
        return "";
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
        return "";
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
                || (inOperation == "drename")  //rename directories
                || (inOperation == "dmove")    //move directories to a different directory
                || (inOperation == "ddelete")  //delete directories
                )
                percentageAllowed = 1000000;
            tempRNG.convertStringToNumber(inFlagValue, percentageAllowed);
        } catch (...) {
            return "Can not parse flag " + inFlag;
        }
        return "";
    }

    if (inFlag == "filetype")
    {
        if ((inFlagValue != "random") && (inFlagValue != "binary") && (inFlagValue != "text"))
            return "filetype must be set to random, binary, or text";
        if (inOperation != "add")
            return "filetype can only be set for add operations";
        return "";
    }

    if (inFlag == "modifytype")
    {
        if (inOperation != "modify")
            return "file_type can only be set for modify operations";
        if (inFlagValue == "random")
            return "";
        try {
            //This function will throw an exception if it can't convert the string
            DDOperationModify::ConvertStringToModifcationType(inFlagValue);
        } catch (...) {
            return "file_type must be set to append, truncate, overwrite, chop, or insert";
        }
        return "";
    }

    if (inFlag == "maxdepth")
    {
        //Only permitted for directory operations
        if ((inOperation != "dadd") && (inOperation != "ddelete") && (inOperation != "drename") && (inOperation != "dmove"))
            return "maxdepth can only be used with directory operations";
        try
        {
            (void)stoul(inFlagValue);
        }
        catch(...)
        {
            return "maxdepth must be number";
        }
        return "";
    }

    return inFlag + " is an unknown flag.";
}

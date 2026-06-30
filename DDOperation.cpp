#include "DDOperation.h"
#include "DDDeterministicPCGPRNG.h"
#include "BS_thread_pool.hpp"

#include "DDOperationAdd.h"

#include <random>

using namespace std;

DDOperation::DDOperation(DDManifest &inManifest) : m_manifest(inManifest)
{
    m_processedSize = 0;
    m_processedCount = 0;

    //Generate the file prefix that is used to generating new files
    m_filePrefix = "DummyDir_v1";
}


std::unique_ptr<DDOperation> DDOperation::getOperation(std::string inOperation, DDManifest &inManifest)
{
    if (inOperation == "add")
        return std::make_unique<DDOperationAdd>(inManifest);
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

    //Reseed the random number generator
    //The parameter contains the seed value stored as a hex string
    m_rng.Seed(stoull(parameters.getFlag("seed"), nullptr, 16));

    //Have the child class do the actual operation
    ChildDoOperation(parameters);

    //Wait for the thread pool to complete
    if (m_threadPool)
        m_threadPool->wait();
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
                || (inOperation == "rename_directory")  //rename directories
                || (inOperation == "move_directory")    //move directories to a different directory
                || (inOperation == "delete_directory")  //delete directories
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
        if ((inFlagValue != "random") || (inFlagValue != "overwrite") && (inFlagValue != "block") && (inFlagValue != "append"))
            return "file_type must be set to random, overwrite, block, or append";
        if (inOperation != "modify")
            return "file_type can only be set for modify operations";
        return "";
    }

    return inFlag + " is an unknown flag.";
}

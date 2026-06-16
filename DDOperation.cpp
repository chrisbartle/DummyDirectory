#include "DDOperation.h"
#include "DDDeterministicPCGPRNG.h"

#include <random>

using namespace std;

DDOperation::DDOperation(DDManifest &inManifest) : m_manifest(inManifest)
{

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
        //If the size is filled in then things won't get out of control
        if (!parameters.isFlag("size"))
        {
            //Perhaps the user has set count and file-size which is a fine way to limit it
            if (!parameters.isFlag("count") && !parameters.isFlag("filesize"))
            {
                //100 megabytes is a good default
                parameters.setFlag("size", "100M");
            }
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

    //Initialize the deterministic random number generator
    DDDeterministicPCGPRNG rng;
    //The parameter contains the seed value stored as a hex string
    rng.Seed(stoull(parameters.getFlag("seed"), nullptr, 16));

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
            (void)stoull(inFlagValue);
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

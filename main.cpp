#include <iostream>
#include <filesystem>

#include "cmake_vals.h"
#include "DDParameters.h"
#include "DDManifest.h"
#include "DDOperation.h"



using namespace std;

void statusCallback(double inPercentage)
{
    cout << "\r" << inPercentage*100 << "%          " << std::flush;
}

void PrintHelp()
{
    cout <<
        R"(Dummy Directory (DummyDir) generates and manipulates a directory tree of files for testing
purposes. It uses a deterministic, seedable pseudo-random number generator, so a run can be
reproduced exactly later from its recorded seed.

USAGE:
    dummydir <operation> <directory> [options]

    <operation>   The operation to perform (see OPERATIONS below)
    <directory>   Path to the dummy directory to operate on. Created if it doesn't exist.

OPERATIONS:
    add       Add new files to the dummy directory
    delete    Delete existing files from the dummy directory
    modify    Modify the contents of existing files
    rename    Rename existing files
    move      Move existing files to a different directory
    dadd      Add new subdirectories
    drename   Rename existing subdirectories
    dmove     Move existing subdirectories to a different parent directory
    ddelete   Delete existing subdirectories, along with everything inside them
    clean     Remove every file and directory this tool has created
    verify    Check that every file on disk matches what the manifest recorded
    rebuild   Rebuild the manifest from what's actually present on disk

OPTIONS:
    --size=<value>       Total number of bytes the operation should affect. Accepts K/M/G/T/P/E
                         suffixes (e.g. 500M, 2.5G), a percentage of the relevant total (e.g.
                         25%), or a range to pick randomly from (e.g. 1k-10m).
    --count=<value>      Total number of items (files or directories) the operation should
                         affect. Accepts the same percentage/range rules as --size.
    --filesize=<value>   For "add", the size of each new file. For "modify", how much of each
                         file's content to change. Same value rules as --size.
    --filetype=<type>    For "add" only: random, binary, or text. Default: random.
    --modifytype=<type>  For "modify" only: append, truncate, overwrite, chop, insert, or
                         random. Default: random.
    --maxdepth=<N>       For "dadd" and "dmove" only: how many levels deep the directory
                         structure is allowed to go. Default: 2.
    --threads=<N>        Number of worker threads to use for file operations. 0 or 1 disables
                         threading. Default: the number of hardware threads available.
    --seed=<hex>         Seed for the pseudo-random number generator, as a hex string. If not
                         given, a random seed is generated and recorded so the run can be
                         reproduced later.
    --verbose            Print details about every individual error, conflict, or discrepancy
                         found, instead of just a summary count. Shorthand: -v
    --replay             Reserved for future use.
    --help               Show this help message and exit.

    Shorthand: -s = --size, -c = --count, -v = --verbose

EXAMPLES:
    dummydir add ./mydir --size=500M --filesize=10M-20M
        Add enough new files to ./mydir to reach 500 megabytes of new data. Each file will be
        between 10 and 20 megabytes in size.

    dummydir delete ./mydir --count=25%
        Delete 25% of the files currently in ./mydir.

    dummydir dmove ./mydir --count=5 --maxdepth=3
        Move 5 random subdirectories to new parent directories, without letting the tree
        exceed 3 levels of nesting.

    dummydir verify ./mydir --verbose
        Check every file against the manifest and print details on anything missing,
        different, or errored.

    dummydir rebuild ./mydir
        Rebuild DummyDir.manifest from what's actually present in ./mydir.
)";
}


int main(int argc, char *argv[])
{
    //We'll do everything, and I mean EVERYTHING, inside this try/catch
    try
    {
        cout << "Dummy Directory version " << APP_VERSION_STRING << endl;
        cout << "Written by Chris Bartle" << endl;

        //Load the parameters
        DDParameters mainParameters;
        mainParameters.LoadFromCommandLine(argc, argv);

        if (mainParameters.isFlag("help"))
        {
            PrintHelp();
            return 0;
        }

        //Is this a replay?
        if (mainParameters.isFlag("replay"))
        {
            //Replay follows special rules:
            //*no operation needs to be specified
            //*the replay flag may or may not include the replay file
            // if it's not there then the replay file is in the dummy directory
        }

        //Make sure there is an operation specified
        if (mainParameters.getOperation().empty())
        {
            cout << "An operation must be provided." << endl;
            cout << "Use --help for the documentation." << endl;
            return 1;
        }
        string operationValidation = DDOperation::ValidateOperationType(mainParameters.getOperation());
        if (operationValidation.length() > 0)
        {
            cout << operationValidation << endl;
            return 1;
        }

        //Validate the flags and make sure that they're valid
        string flagValidation = mainParameters.validateFlags();
        if (flagValidation.length() > 0)
        {
            cout << flagValidation << endl;
            return 1;
        }

        //Get the directory from the parameter. Does it exist? Is it a directory?
        if (mainParameters.getDirectoryPath().empty())
        {
            cout << "A valid directory path must be provided." << endl;
            return 1;
        }
        std::filesystem::path absoluteDirectoryPath = mainParameters.getAbsoluteDirectoryPath();
        if (!std::filesystem::exists(absoluteDirectoryPath))
        {
            //It does not exist so create it
            if (!std::filesystem::create_directories(absoluteDirectoryPath))
            {
                cout << "The provided path:" << endl << absoluteDirectoryPath << endl;
                cout << "does not exist and could not be created." << endl;
                return 1;
            }
            cout << absoluteDirectoryPath << " was created." << endl;
        }

        //The DummyDir.manifest file always sits in the root of the dummy directory.
        std::filesystem::path absoluteManifestPath = absoluteDirectoryPath / "DummyDir.manifest";
        DDManifest manifest;
        manifest.SetFilepath(absoluteManifestPath);
        if (mainParameters.getOperation() != "rebuild")
        {
            manifest.LoadFromFile();
            cout << manifest.GetManifestSummation() << endl;
        }

        //Perform the operation
        unique_ptr<DDOperation> operation = DDOperation::getOperationByName(mainParameters.getOperation(), manifest);
        operation->SetDefaultParameters(mainParameters);
        operation->setStatusCallbackFunction(statusCallback);
        cout << "Processing..." << endl;
        operation->DoOperation(mainParameters);
        //Use a carriage return to clear the percentage indicator
        cout << "\r" << operation->GetOperationSummation() << endl;

        //Look for errors
        if (mainParameters.getOperation() == "verify")
        {
            //Verification has custom output
            uint64_t missingCount = 0;
            uint64_t differentCount = 0;
            uint64_t errorCount = 0;
            for (uint64_t eloop = 0; eloop < manifest.getTotalFileCount(); eloop++)
            {
                DDFile& thisFile = manifest.getFileByPos(eloop);
                if (thisFile.processingStatus() == DDFile::ERROR)
                {
                    errorCount++;
                    if (mainParameters.isFlag("verbose"))
                        cout << thisFile.relativePathname().string() << " threw error " << thisFile.getProcessingError() << endl;
                }
                else if (thisFile.processingStatus() == DDFile::MISSING)
                {
                    missingCount++;
                    if (mainParameters.isFlag("verbose"))
                        cout << thisFile.relativePathname().string() << " is missing!" << endl;
                }
                else if (thisFile.processingStatus() == DDFile::DIFFERENT)
                {
                    differentCount++;
                    if (mainParameters.isFlag("verbose"))
                        cout << thisFile.relativePathname().string() << " has a different hash!" << endl;
                }
            }
            if ((missingCount == 0) && (differentCount == 0) && (errorCount == 0))
                cout << "All files were successfully validated!" << endl;
            else
            {
                if (missingCount > 0)
                    cout << missingCount << " files listed on the manifest are missing from the file system" << endl;
                if (differentCount > 0)
                    cout << differentCount << " files have a different hash" << endl;
                if (errorCount > 0)
                    cout << errorCount << " files could not be processed due to an error" << endl;
                if (!mainParameters.isFlag("verbose"))
                    cout << "Re-run verify with --verbose flag to get a list of specific files" << endl;
            }
        }
        else
        {
            //With other operations, we only care about errors and conflicts
            uint64_t errorCount = 0;
            uint64_t conflictCount = 0;
            for (uint64_t eloop = 0; eloop < manifest.getTotalFileCount(); eloop++)
            {
                DDFile& thisFile = manifest.getFileByPos(eloop);
                if (thisFile.processingStatus() == DDFile::ERROR)
                {
                    errorCount++;
                    if (mainParameters.isFlag("verbose"))
                        cout << thisFile.relativePathname().string() << " threw error " << thisFile.getProcessingError() << endl;
                }
                else if (thisFile.processingStatus() == DDFile::CONFLICT)
                {
                    conflictCount++;
                    if (mainParameters.isFlag("verbose"))
                        cout << thisFile.relativePathname().string() << " could not be added due to an existing file with the same name" << endl;
                }
            }
            if ((errorCount > 0) || (conflictCount > 0))
            {
                if (conflictCount > 0)
                    cout << conflictCount << " files could not be added because the file already existed" << endl;
                if (errorCount > 0)
                    cout << errorCount << " files could not be processed due to an error" << endl;
                if (!mainParameters.isFlag("verbose"))
                    cout << "Re-run with --verbose flag to get a list of specific files" << endl;
            }
        }
        manifest.PostOperationCleanup();

        if (mainParameters.getOperation() != "verify")
        {
            if (mainParameters.getOperation() == "clean")
            {
                cout << "Cleaning complete!" << endl;
            }
            else
            {
                cout << "Processing complete!" << endl;
                cout << manifest.GetManifestSummation() << endl;
            }

            //Save the new manifest
            manifest.SaveToFile();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Critical Error (std::exception): " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Critical Error: An unknown, non-standard exception occurred." << std::endl;
        return 2;
    }

    return 0;
}

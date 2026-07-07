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
        manifest.LoadFromFile();
        cout << manifest.GetManifestSummation() << endl;

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

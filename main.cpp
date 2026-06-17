#include <iostream>
#include <filesystem>

#include "cmake_vals.h"
#include "DDParameters.h"
#include "DDManifest.h"
#include "DDOperation.h"

using namespace std;

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
        std::filesystem::path relativeDirectoryPath = mainParameters.getDirectoryPath();
        std::filesystem::path absoluteDirectoryPath = std::filesystem::absolute(relativeDirectoryPath).lexically_normal();
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

        //Perform the operation
        DDOperation operation(manifest);
        operation.SetDefaultParameters(mainParameters);
        operation.DoOperation(mainParameters);

        //Save the new manifest
        manifest.SaveToFile();
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

#include "DDParameters.h"

DDParameters::DDParameters() {}

/**
 * @brief DDParameters::LoadFromCommandLine
 * Loads the parameters from the command line.
 * Two arguments, the operation and the dummy directory path may be provided
 * Additional flags can be set
 * @param argc
 * @param argv
 */
void DDParameters::LoadFromCommandLine(int argc, char* argv[])
{
    // Loop through arguments (skip argv[0] which is the program name)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Check if it's an option (starts with '-')
        if (arg[0] == '-') {
            // Handle standard key=value syntax (e.g., --output=dir)
            size_t equals_pos = arg.find('=');
            if (equals_pos != std::string::npos) {
                std::string key = arg.substr(0, equals_pos);
                std::string val = arg.substr(equals_pos + 1);
                _flags[key] = val;
            }
            // Handle space-separated key value (e.g., -o dir)
            else if (i + 1 < argc && argv[i + 1][0] != '-') {
                _flags[arg] = argv[i + 1];
                static_cast<void>(++i); // Skip the next index since we just consumed it as a value
            }
            // It's a standalone boolean flag (e.g., --verbose)
            else {
                _flags[arg] = "true";
            }
        }
        // If it doesn't start with '-', process it immediately as an argument!
        else {
            _arguments.push_back(arg);
        }
    }
}

/**
 * @brief DDParameters::isFlag
 * Returns true if the command line flag exists (flags start with - or -- and may be set to a value)
 * @param inFlagName Name of the flag with dashes removed
 * @return true if the flag exists
 */
bool DDParameters::isFlag(std::string inFlagName)
{
    return _flags.contains(inFlagName);
}

/**
 * @brief DDParameters::getFlag
 * Returns the value of a command line flag (flags start with - or -- and may be set to a value)
 * @param inFlagName Name of the flag with dashes removed
 * @return The value that the flag is set to
 */
std::string DDParameters::getFlag(std::string inFlagName)
{
    return _flags[inFlagName];
}

/**
 * @brief DDParameters::getOperation
 * Returns the requested operation
 * @return The operation, empty if the argument wasn't provided (or if replay flag was set)
 */
std::string DDParameters::getOperation()
{
    //If the replay flag is used then the operation isn't provided and the first argument
    //becomes the directory path
    if (isFlag("replay"))
        return "";
    if (_arguments.size() < 1)
        return "";
    return _arguments[0];
}

/**
 * @brief DDParameters::getDirectoryPath
 * Returns the path to the dummy directory
 * @return The path of the dummy directory, empty if the argument wasn't provided
 */
std::string DDParameters::getDirectoryPath()
{
    //If the replay flag is used then the first argument is the directory path not the operation
    if (isFlag("replay"))
    {
        if (_arguments.size() == 0)
            return "";
        return _arguments[0];
    }
    if (_arguments.size() < 2)
        return "";
    return _arguments[1];
}
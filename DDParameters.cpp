#include "DDParameters.h"
#include "DDOperation.h"

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
                setFlag(key, val);
            }
            // Handle space-separated key value (e.g., -o dir)
            else if (i + 1 < argc && argv[i + 1][0] != '-') {
                //Some flags don't take a value
                if (arg == "--verbose")
                    setFlag(arg, "");
                else
                {
                    setFlag(arg, argv[i + 1]);
                    static_cast<void>(++i); // Skip the next index since we just consumed it as a value
                }
            }
            // It's a standalone boolean flag (e.g., --verbose)
            else {
                setFlag(arg, "");
            }
        }
        // If it doesn't start with '-', process it immediately as an argument!
        else {
            m_arguments.push_back(arg);
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
    return m_flags.contains(inFlagName);
}

/**
 * @brief DDParameters::getFlag
 * Returns the value of a command line flag (flags start with - or -- and may be set to a value)
 * @param inFlagName Name of the flag with dashes removed
 * @return The value that the flag is set to
 */
std::string DDParameters::getFlag(std::string inFlagName)
{
    return m_flags[inFlagName];
}

/**
 * @brief DDParameters::setFlag
 * This can be used to change the value of certain flags or clean them up if needed
 * @param inFlagName Name of the flag. -- are stripped from the flag name. Short flags are converted to their long form.
 * @param inFlag The value of the flag
 */
void DDParameters::setFlag(std::string inFlagName, std::string inFlag)
{
    std::string flagName;
    //Strip the --
    if (inFlagName.starts_with("--"))
        flagName = inFlagName.substr(2, inFlagName.size()-2);
    else
        flagName = inFlagName;
    m_flags[flagName] = inFlag;
}

/**
 * @brief DDParameters::validateFlags
 * Iterate through all of the flags and confirm that they are all valid
 * @return Empty string indicates that all flags are valid otherwise a multi-line
 * string is returned with a list of found problems. This can be shown to the user
 */
std::string DDParameters::validateFlags()
{
    std::string validationErrors = "";
    for (const auto& flag : m_flags)
    {
        std::string validationError = DDOperation::ValidateFlag(getOperation(), flag.first, flag.second);
        if (!validationError.empty())
            validationErrors = validationError + "\n";
    }
    return validationErrors;
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
    if (m_arguments.size() < 1)
        return "";
    return m_arguments[0];
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
        if (m_arguments.size() == 0)
            return "";
        return m_arguments[0];
    }
    if (m_arguments.size() < 2)
        return "";
    return m_arguments[1];
}
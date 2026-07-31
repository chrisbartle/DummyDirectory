#include "DDReplay.h"
#include "cmake_vals.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

using namespace std;

DDReplay::DDReplay()
{

}

DDReplay::~DDReplay()
{
    CloseForWrite();
}

/**
 * @brief DDReplay::WriteOperation
 * Logs the operation that occured and its parameters
 * @param inParameters
 */
void DDReplay::WriteOperation(DDParameters inParameters)
{
    m_currentOperation = inParameters.getOperation();
    //verify and rebuild operations shouldn't change anything and thus don't need to be logged
    if ((m_currentOperation == "verify") || (m_currentOperation == "rebuild"))
        return;

    OpenForWrite();

    //The operation line is formated:
    //operation flag1=value1 flag2=value2 etc...
    m_replayFileWriter << m_currentOperation;
    for (const auto& flag : inParameters.getFlagMap())
    {
        //Skip these flags
        if ((flag.first == "verbose") or (flag.first == "threads"))
            continue;

        if (flag.second.empty())
            //If there is no value then just produce the flag
            m_replayFileWriter << " --" << flag.first;
        else
            m_replayFileWriter << " --" << flag.first << "=" << flag.second;
    }
    m_replayFileWriter << endl;
}

/**
 * @brief DDReplay::WriteComment
 * Write out a comment, prepended with #
 * @param inComment String that contains the comment
 */
void DDReplay::WriteComment(std::string inComment)
{
    //verify and rebuild operations shouldn't change anything and thus don't need to be logged
    if ((m_currentOperation == "verify") || (m_currentOperation == "rebuild"))
        return;

    OpenForWrite();

    //Since every line needs to start with #, we need to find any newlines and append # there as well
    string outComment;
    outComment.reserve(inComment.length());
    for (char c : inComment)
    {
        outComment += c;
        if (c == '\n')
            outComment += "# ";
    }
    m_replayFileWriter << "# " << outComment << endl;
}

std::vector<string> DDReplay::ReadOperations()
{
    vector<string> operationList;

    //We don't want it open for writing as we're about to read from it
    CloseForWrite();

    //If file doesn't exist then nothing to load
    if (!std::filesystem::is_regular_file(m_absoluteReplayPath))
        return operationList;

    //Open a stream and start reading
    std::ifstream inReplayFile(m_absoluteReplayPath, std::ios::binary);

    //Loop through all of the lines
    std::string lineString;
    uint64_t lineNumber = 0;
    while (std::getline(inReplayFile, lineString))
    {
        lineNumber++;
        try
        {
            //Any line that starts with a # should be ignored.
            if (lineString.empty() || lineString.starts_with('#'))
                continue;
            operationList.push_back(lineString);
        }
        catch(...)
        {
            throw std::runtime_error("Unable to read line " + std::to_string(lineNumber) + " in " + m_absoluteReplayPath.string());
        }
    }
    inReplayFile.close();

    return operationList;
}

void DDReplay::OpenForWrite()
{
    if (m_replayFileWriter.is_open())
        return;

    //See if the file exists
    bool replayExists = std::filesystem::exists(m_absoluteReplayPath);

    m_replayFileWriter.open(m_absoluteReplayPath, std::ios::app | std::ios::binary);

    if (!replayExists)
    {
        //Every replay file should start with these lines
        m_replayFileWriter << "# Dummy Directory replay file" << endl;
        m_replayFileWriter << "# Version " << APP_VERSION_STRING << endl;
    }
}

void DDReplay::CloseForWrite()
{
    if (m_replayFileWriter.is_open())
        m_replayFileWriter.close();
}

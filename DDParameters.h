#ifndef DDPARAMETERS_H
#define DDPARAMETERS_H

#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>

class DDParameters
{
public:
    DDParameters();

    void LoadFromCommandLine(int argc, char* argv[]);
    void LoadFromReplay(std::string inReplayOperation, std::filesystem::path inAbsoluteDirectoryPath);

    bool isFlag(std::string inFlagName);
    std::string getFlag(std::string inFlagName);
    void setFlag(std::string inFlagName, std::string inFlag);
    std::string validateFlags();

    std::string getOperation();
    std::string getDirectoryPath();
    int getNumberOfArguments() { return m_arguments.size(); }
    std::filesystem::path getAbsoluteDirectoryPath();
    std::filesystem::path ConvertToAbsolutePath(std::filesystem::path inPath) {return getAbsoluteDirectoryPath() / inPath; }

    std::unordered_map<std::string, std::string> &getFlagMap() { return m_flags; }

private:
    std::unordered_map<std::string, std::string> m_flags;
    std::vector<std::string> m_arguments;
    std::filesystem::path m_absoluteDirectoryPath;
};

#endif // DDPARAMETERS_H

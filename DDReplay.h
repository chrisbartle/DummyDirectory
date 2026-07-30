#ifndef DDREPLAY_H
#define DDREPLAY_H

#include <filesystem>
#include <fstream>
#include "DDParameters.h"

class DDReplay
{
public:
    DDReplay();
    ~DDReplay();

    void SetFilepath(std::filesystem::path inFilepath) { m_absoluteReplayPath = inFilepath; };
    void WriteOperation(DDParameters inParameters);
    void WriteComment(std::string inComment);

private:
    void OpenForWrite();

    std::filesystem::path m_absoluteReplayPath;
    std::ofstream m_replayFileWriter;
    std::string m_currentOperation;
};

#endif // DDREPLAY_H

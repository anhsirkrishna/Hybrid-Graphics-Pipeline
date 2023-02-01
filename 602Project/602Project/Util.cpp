#include "Util.h"

#include <fstream>

std::string LoadFileIntoString(const std::string& filename) {
    std::string   result;
    std::ifstream stream(filename, std::ios::ate | std::ios::binary);  //ate: Open at file end

    if (!stream.is_open())
        return result;

    result.reserve(stream.tellg()); // tellg() is last char position in file (i.e.,  length)
    stream.seekg(0, std::ios::beg);

    result.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    return result;
}

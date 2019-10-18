
#include "FileUtils.h"

#include <string>
#include <fstream>
#include <streambuf>
#include <src/util/Logging.h>

namespace miner {namespace file {

        template<class T>
        inline static optional<T> readFileInto(const std::string &filePath) {
            using namespace std;
            ifstream stream(filePath);

            if (stream) {
                T result;

                stream.seekg(0, ios::end);
                auto size = stream.tellg();
                stream.seekg(0, ios::beg);

                if (size != -1) {
                    result.reserve(size);
                    result.assign((istreambuf_iterator<char>(stream)),
                                  istreambuf_iterator<char>());
                    return result;
                }
                else {
                    LOG(WARNING) << "Failed to obtain size of file at '" << filePath << "'";
                }
            }
            else {
                LOG(WARNING) << "Failed when trying to load a file at '" << filePath << "'";
            }
            return nullopt;
        }

        optional<std::string> readFileIntoString(const std::string &filePath) {
            return readFileInto<std::string>(filePath);
        }

        optional<std::vector<uint8_t>> readFileIntoByteVector(const std::string &filePath) {
            return readFileInto<std::vector<uint8_t>>(filePath);
        }

        bool writeStringIntoFile(const std::string &path, const std::string &content) {
            std::ofstream file(path);
            if (!file.good())
                return false;
            file << content;
            file.close();
            return true;
        }

    }}
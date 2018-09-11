
#include "FileUtils.h"

#include <string>
#include <fstream>
#include <streambuf>
#include <src/util/Logging.h>

namespace miner {namespace file {

        optional<std::string> readFileIntoString(cstring_span filePath) {
            using namespace std;
            ifstream stream(filePath.data());

            if (stream) {
                string result;

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
                    LOG(WARNING) << "Failed to obtain size of file at '"
                               << to_string(filePath) << "'";
                }
            }
            else {
                LOG(WARNING) << "Failed when trying to load a file at '"
                << to_string(filePath) << "'";
            }
            return nullopt;
        }

    }}
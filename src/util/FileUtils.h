
#pragma once

#include <src/common/Optional.h>
#include <vector>

namespace riner {namespace file {

        /**
         * creates a std::string from the contents of a file
         * @param filePath path to the file
         * @return std::string with contents of file, nullopt if file couldn't be read (e.g. if it doesn't exist)
         */
        optional<std::string> readFileIntoString(const std::string &filePath);

        /**
         * creates a std::vector<uint8_t> containing the bytes of a file
         * @param filePath path to the file
         * @return std::vector<uint8_t> with contents of file, nullopt if file couldn't be read (e.g. if it doesn't exist)
         */
        optional<std::vector<uint8_t>> readFileIntoByteVector(const std::string &filePath);

        /**
         * convenience function for writing a string to a file, see implementation for details
         * @param filePath path of the file
         * @param content string content to be written into the file
         * @return bool indicating if write was successful (true) or not (false)
         */
        bool writeStringIntoFile(const std::string &filePath, const std::string &content);

}}

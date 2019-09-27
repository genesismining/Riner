
#pragma once

#include <src/common/Optional.h>
#include <vector>

namespace miner {namespace file {

        optional<std::string> readFileIntoString(const std::string &filePath);

        optional<std::vector<uint8_t>> readFileIntoByteVector(const std::string &filePath);



}}

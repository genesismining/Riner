
#pragma once

#include <src/common/StringSpan.h>
#include <src/common/Optional.h>
#include <vector>

namespace miner {namespace file {

        optional<std::string> readFileIntoString(cstring_span filePath);

        optional<std::vector<uint8_t>> readFileIntoByteVector(cstring_span filePath);



}}

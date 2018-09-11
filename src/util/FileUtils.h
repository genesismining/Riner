
#pragma once

#include <src/common/StringSpan.h>
#include <src/common/Optional.h>

namespace miner {namespace file {

        optional<std::string> readFileIntoString(cstring_span filePath);

}}
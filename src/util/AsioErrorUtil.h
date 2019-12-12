//
//

#pragma once

#include <string>
#include <asio.hpp>

namespace miner {

    std::string asio_error_name(const asio::error_code &);

    std::string asio_error_name_num(const asio::error_code &);

}


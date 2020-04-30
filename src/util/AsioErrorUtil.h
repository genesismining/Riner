//
//

#pragma once

#include <string>
#include <asio.hpp>

namespace riner {

    /**
     * @brief get a printable name for an asio error_code
     * @returns asio::error_code enums as string
     */
    std::string asio_error_name(const asio::error_code &);

    /**
     * @brief returns asio_error_name (see function above) but with the appended error code number
     * @returns asio::error_code enums as string with error code number added
     */
    std::string asio_error_name_num(const asio::error_code &);

}


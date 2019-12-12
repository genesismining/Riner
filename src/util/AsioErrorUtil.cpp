//
//

#include "AsioErrorUtil.h"

namespace miner {

#define ASIO_ERR_CASE(x) case asio::error:: x: return "error::" #x ;

    std::string asio_error_name(const asio::error_code &e) {
        switch(e.value()) {
            case 0: return "success";
            //case (asio::error::(access_denied): return "access_deined";
            ASIO_ERR_CASE(access_denied)
            ASIO_ERR_CASE(address_family_not_supported)
            ASIO_ERR_CASE(address_in_use)
            ASIO_ERR_CASE(already_connected)
            ASIO_ERR_CASE(already_started)
            ASIO_ERR_CASE(broken_pipe)
            ASIO_ERR_CASE(connection_aborted)
            ASIO_ERR_CASE(connection_refused)
            ASIO_ERR_CASE(connection_reset)
            ASIO_ERR_CASE(bad_descriptor)
            ASIO_ERR_CASE(fault)
            ASIO_ERR_CASE(host_unreachable)
            ASIO_ERR_CASE(in_progress)
            ASIO_ERR_CASE(interrupted)
            ASIO_ERR_CASE(invalid_argument)
            ASIO_ERR_CASE(message_size)
            ASIO_ERR_CASE(name_too_long)
            ASIO_ERR_CASE(network_down)
            ASIO_ERR_CASE(network_reset)
            ASIO_ERR_CASE(network_unreachable)
            ASIO_ERR_CASE(no_descriptors)
            ASIO_ERR_CASE(no_buffer_space)
            ASIO_ERR_CASE(no_memory)
            ASIO_ERR_CASE(no_permission)
            ASIO_ERR_CASE(no_protocol_option)
            ASIO_ERR_CASE(not_connected)
            ASIO_ERR_CASE(not_socket)
            ASIO_ERR_CASE(operation_aborted)
            ASIO_ERR_CASE(operation_not_supported)
            ASIO_ERR_CASE(shut_down)
            ASIO_ERR_CASE(timed_out)
            ASIO_ERR_CASE(try_again)
            //ASIO_ERR_CASE(would_block) //same error code as "try again"
            default: return "unknown error";
        }
    }

    std::string asio_error_name_num(const asio::error_code &e) {
        return asio_error_name(e) + " (#" + std::to_string(e.value()) + ")";
    }
}

/*
 *
 */
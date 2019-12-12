#pragma once
constexpr const char *defaultConfigCStr = R":::(

# the following text shows the default values which are used for fields that are not specified in a config file
# Do not use this file as an example config. Take a look at 'ExampleConfig.textproto' instead
# BEGIN OF DEFAULT CONFIG

version: "0.1"

global_settings {
    api_port: 4028
    opencl_kernel_dir: ""
    start_profile_name: "example_profile"
}

profile {
    name: "example_profile"

    task {
        run_on_all_remaining_devices: true
        use_device_profile_with_name: "my_gpu_profile"
        run_algoimpl_with_name: "AlgoEthashCL"
    }

    task {
        device_index: 1
        use_device_profile_with_name: "my_gpu_profile"
        run_algoimpl_with_name: "AlgoCuckatoo31"
    }
}

device_profile {
    name: "my_gpu_profile"

    settings_for_algoimpl {
        key: "AlgoEthashCL"

        value: {
            work_size: 1024
        }
    }

    settings_for_algoimpl {
        key: "AlgoCuckatoo31"

        value: {
            work_size: 512
        }
    }
}

pool {
    pow_type: "ethash"
    host: "127.0.0.1"
    port: 2345
    username: "a"
    password: "x"
}

pool {
    pow_type: "grin"
    host: "127.0.0.1"
    port: 2346
    username: "b"
    password: "y"
}

# END OF DEFAULT CONFIG

):::";
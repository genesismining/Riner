#pragma once
constexpr auto tutorial_config = R"(
#
# this config is used by the --run-tutorial command which runs the AlgoDummy
# and PoolDummy code to show how riner functions.
# if you want to understand more about Config files, read "src/config/ExampleConfig.textproto"
# which contains lots of comments
#

version: "0.1"

global_settings {
  opencl_kernel_dir: "kernel/dir/"
  start_profile_name: "dummy_profile"
}

profile {name: "dummy_profile"

  task {
    run_on_all_remaining_devices: true
    run_algoimpl_with_name: "AlgoDummy"
    use_device_profile_with_name: "gpu_profile"
  }
}

device_profile {name: "gpu_profile"

  settings_for_algoimpl {key: "AlgoDummy" value {
      num_threads: 1
      work_size: 1024
  }}
}

pool {
  pow_type: "dummy"
  protocol: "stratum2"
  host: "127.0.0.1"
  port: 2345
  username: "user"
  password: "password"
}

)";
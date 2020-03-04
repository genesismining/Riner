

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <src/util/FileUtils.h>
#include <src/application/Application.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>
#include <src/util/StringUtils.h>

#include <config.pb.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <src/config/ConfigDefaultValues.h>

namespace riner {
    using namespace proto;

    TEST(Config, ParseDefaults) {
        using TextFormat = google::protobuf::TextFormat;
        std::string str = defaultConfigCStr;

        Config config;
        EXPECT_TRUE(TextFormat::ParseFromString(str, &config));

        std::string str2;
        EXPECT_TRUE(TextFormat::PrintToString(config, &str2));

        LOG(INFO) << "stripped default config: " << str2;

        Config config2;
        EXPECT_TRUE(TextFormat::ParseFromString(str2, &config2));

        EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(config, config2));
    }

    TEST(Config, ConcatPath) {
        EXPECT_EQ(concatPath("path/to", "file"), "path/to/file");
        EXPECT_EQ(concatPath("path/to/", "file"), "path/to/file");
        EXPECT_EQ(concatPath("path/to", ""), "path/to/");
        EXPECT_EQ(concatPath("", ""), "");
        EXPECT_EQ(concatPath("/", ""), "/");

        using E = std::invalid_argument;
        EXPECT_THROW(concatPath("path/to"  ,"/file"), E);
        EXPECT_THROW(concatPath("path/to/" ,"/file"), E);
        EXPECT_THROW(concatPath("path/to/" ,"/"), E);
        EXPECT_THROW(concatPath("path/to"  ,"/"), E);
        EXPECT_THROW(concatPath(""  , "/"), E);
        EXPECT_THROW(concatPath("/" , "/"), E);
    }

    TEST(Config, LaunchApp) {
        optional<Config> config = parseConfig(R"(version: "0.1"
global_settings {
    api_port: 4028
    opencl_kernel_dir: ""
    start_profile_name: "prof_0"
}

profile {
    name: "prof_0"

    task {
      run_on_all_remaining_devices: true
      run_algoimpl_with_name: "AlgoDummy"
      use_device_profile_with_name: "dprof_0"
    }
}

device_profile {
    name: "dprof_0"
    settings_for_algoimpl {
        key: "AlgoDummy"
        value {}
    }
}

pool {
    pow_type: "ethash"
    protocol: "dummy"
    host: "127.0.0.1"
    port: 2345
    username: "a"
    password: "x"
}
)");
        ASSERT_TRUE(config); //do not proceed unless config exists
        EXPECT_EQ(config->global_settings().api_port(), 4028);

        {
            Application app{*config};

            {
                auto lock = app.devicesInUse.lock();
                for (auto &dev : *lock) {
                    if (dev)
                        LOG(INFO) << "algoimpl of device " << dev->id.getName() << ": " << dev->settings.algoImplName;
                }
            }
            EXPECT_EQ(app.algorithms.size(), 1);
        }//Application dtor
    }

    TEST(Config, GenerateProtoMessage) {

        Config c;
        c.set_version("0.1");

        {//Global Settings
            auto &g = *c.mutable_global_settings();
            g.set_api_port(4028);
            g.set_opencl_kernel_dir("kernel/dir/");
            g.set_start_profile_name("example_profile");
        }

        {//Profile
            auto &profile = *c.add_profile();
            profile.set_name("my_profile");

            { //Task 0
                auto &task = *profile.add_task();
                task.set_run_on_all_remaining_devices(true);
                task.set_use_device_profile_with_name("my_gpu_profile");
                task.set_run_algoimpl_with_name("EthashCL");
            }

            { //Task 1
                auto &task = *profile.add_task();
                task.set_device_index(1);
                task.set_use_device_profile_with_name("my_gpu_profile");
                task.set_run_algoimpl_with_name("Cuckatoo31");
            }
        }

        {//Device Profile
            auto &d = *c.add_device_profile();
            d.set_name("my_gpu_profile");

            auto &settings_for_algoimpl = *d.mutable_settings_for_algoimpl();

            {//AlgoSettings 0
                auto &a = settings_for_algoimpl["EthashCL"];
                a.set_work_size(1024);
                a.set_num_threads(4);
            }

            {//AlgoSettings 1
                auto &a = settings_for_algoimpl["Cuckatoo31"];
                a.set_work_size(2048);
            }
        }

        {//Pool
            auto &pool = *c.add_pool();
            pool.set_pow_type("ethash");
            pool.set_protocol("stratum2");
            pool.set_host("127.0.0.1");
            pool.set_port(2345);
            pool.set_username("a");
            pool.set_password("x");
        }

        {//Pool
            auto &pool = *c.add_pool();
            pool.set_pow_type("cuckatoo31");
            pool.set_protocol("stratum");
            pool.set_host("127.0.0.1");
            pool.set_port(2346);
            pool.set_username("b");
            pool.set_password("y");
        }

        std::string text;

        c.SerializeToString(&text);
        google::protobuf::TextFormat::PrintToString(c, &text);

        LOG(INFO) << "serialized to text: " << text;

        EXPECT_TRUE(true);
    }


}

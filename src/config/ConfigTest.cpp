

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <src/util/FileUtils.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>

#include <config.pb.h>

#include <google/protobuf/text_format.h>




namespace miner {

    TEST(Config, GenerateProtoMessage) {
        using namespace proto;

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
                task.set_run_algoimpl_with_name("AlgoEthashCL");
            }

            { //Task 1
                auto &task = *profile.add_task();
                task.set_device_index(1);
                task.set_use_device_profile_with_name("my_gpu_profile");
                task.set_run_algoimpl_with_name("AlgoCuckatoo31");
            }
        }

        {//Device Profile
            auto &d = *c.add_device_profile();
            d.set_name("my_gpu_profile");

            auto &settings_for_algoimpl = *d.mutable_settings_for_algoimpl();

            {//AlgoSettings 0
                auto &a = settings_for_algoimpl["AlgoEthashCL"];
                a.set_work_size(1024);
                a.set_num_threads(4);
            }

            {//AlgoSettings 1
                auto &a = settings_for_algoimpl["AlgoCuckatoo31"];
                a.set_work_size(2048);
            }
        }

        {//Pool
            auto &pool = *c.add_pool();
            pool.set_pow_type("ethash");
            pool.set_host("127.0.0.1");
            pool.set_port(2345);
            pool.set_username("a");
            pool.set_password("x");
        }

        {//Pool
            auto &pool = *c.add_pool();
            pool.set_pow_type("grin");
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
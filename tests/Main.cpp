
#include <gtest/gtest.h>
#include <Global.h>
#include <Logging.h>

//#define SE_IMPL
//#include <GlobalTypes.h>

std::shared_ptr<spdlog::logger> gLogger = spdlog::stdout_logger_mt("T");

int main(int argc, char** argv) {

        log_i("Running {}", argv[0]);
        testing::InitGoogleTest(&argc, argv);

        //log_d ("init remain engine subsytems");
        //SE::TEngine::Instance().Init();

        return RUN_ALL_TESTS();
}


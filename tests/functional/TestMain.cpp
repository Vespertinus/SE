
#include <gtest/gtest.h>
#include <Logging.h>

// The engine logging backend. Functional tests swap gLogger via se_test::LogCapture
// per test; this logger stays the process-wide default outside captures.
std::shared_ptr<spdlog::logger> gLogger = spdlog::stdout_logger_mt("T");

int main(int argc, char** argv) {

        log_i("Running {}", argv[0]);
        testing::InitGoogleTest(&argc, argv);

        return RUN_ALL_TESTS();
}

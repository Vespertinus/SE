
#include <gtest/gtest.h>
#include <Logging.h>

#include <csignal>
#include <cstdio>
#include <execinfo.h>

namespace {
void SegvBacktrace(int sig) {
        void * vAddrs[32];
        const int n = backtrace(vAddrs, 32);
        fprintf(stderr, "\n*** signal %d, backtrace (%d frames) ***\n", sig, n);
        backtrace_symbols_fd(vAddrs, n, 2);
        _exit(139);
}
}

// The engine logging backend. Functional tests swap gLogger via se_test::LogCapture
// per test; this logger stays the process-wide default outside captures.
std::shared_ptr<spdlog::logger> gLogger = spdlog::stdout_logger_mt("T");

int main(int argc, char** argv) {

        signal(SIGSEGV, SegvBacktrace);
        log_i("Running {}", argv[0]);
        testing::InitGoogleTest(&argc, argv);

        return RUN_ALL_TESTS();
}


// Bench binary entry point support: the engine logging backend.
// main() comes from benchmark::benchmark_main (see tests/CMakeLists.txt);
// benchmarks that need the engine initialize it lazily via BenchAssets.h.
//
// NOTE (ODR): like every engine-based binary here, the bench is ONE TU for
// the SE_IMPL blocks — AnimBench.cpp holds them.

#include <Logging.h>

std::shared_ptr<spdlog::logger> gLogger = spdlog::stdout_logger_mt("B");

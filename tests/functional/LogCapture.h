
#ifndef SE_TEST_LOG_CAPTURE_H
#define SE_TEST_LOG_CAPTURE_H

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/callback_sink.h>

#include <Logging.h>

namespace se_test {

// ---------------------------------------------------------------------------
// LogCapture — RAII capture of engine log output.
//
// Swaps the global gLogger for a logger backed by a spdlog callback sink and
// restores the previous logger on destruction, so captures nest safely.
//
// This is the harness' observable-failure primitive: the engine fails softly
// (logs an error, keeps ticking, returns invalid handles) and without capture
// those degradations are invisible to tests.  UE analogs: automation tests fail
// on log errors; Unity has LogAssert.Expect/Fails.
//
// Stored line format: "[level] message" where message is already the fully
// formatted engine text ("FUNC: msg (file:line)").
// ---------------------------------------------------------------------------
class LogCapture {

public:
        explicit LogCapture(spdlog::level::level_enum min_level = spdlog::level::trace)
                : pPrevious(gLogger) {

                pSink = std::make_shared<spdlog::sinks::callback_sink_mt>(
                                [this](const spdlog::details::log_msg& oMsg) {
                                        vLines.emplace_back(
                                                        fmt::format("[{}] {}",
                                                                        spdlog::level::to_string_view(oMsg.level),
                                                                        oMsg.payload));
                                        if (oMsg.level >= spdlog::level::err) {
                                                ++error_count;
                                        } else if (oMsg.level >= spdlog::level::warn) {
                                                ++warn_count;
                                        }
                                });

                auto pLogger = std::make_shared<spdlog::logger>("se_test_capture", pSink);
                pLogger->set_level(min_level);
                // Unregistered in spdlog's registry on purpose: repeated
                // construct/destroy must never hit "logger name already exists".
                gLogger = std::move(pLogger);
        }

        ~LogCapture() noexcept {
                gLogger = pPrevious;
        }

        LogCapture(const LogCapture&) = delete;
        LogCapture& operator=(const LogCapture&) = delete;

        const std::vector<std::string>& Lines() const { return vLines; }
        int  ErrorCount()   const noexcept { return error_count; }
        int  WarningCount() const noexcept { return warn_count;  }

        // ---- GTest-failing (non-fatal) expectations --------------------------

        // The workhorse: no log_e during the captured scope.
        void ExpectNoErrors() const {
                if (error_count == 0) {
                        EXPECT_EQ(0, error_count);
                        return;
                }
                std::string sFirst;
                for (const auto& sLine : vLines) {
                        if (sLine.rfind("[error]", 0) == 0) { sFirst = sLine; break; }
                }
                EXPECT_EQ(0, error_count) << "first error: " << sFirst;
        }

        void ExpectNoWarnings() const {
                EXPECT_EQ(0, warn_count);
        }

        void ExpectLine(std::string_view sSub) const {
                for (const auto& sLine : vLines) {
                        if (sLine.find(sSub) != std::string::npos) {
                                SUCCEED() << "found: " << sLine;
                                return;
                        }
                }
                ADD_FAILURE() << "expected a log line containing '" << sSub << "'";
        }

        void ExpectNoLine(std::string_view sSub) const {
                for (const auto& sLine : vLines) {
                        if (sLine.find(sSub) != std::string::npos) {
                                ADD_FAILURE() << "unexpected log line: " << sLine;
                                return;
                        }
                }
        }

        void Clear() {
                vLines.clear();
                error_count = 0;
                warn_count  = 0;
        }

private:
        std::shared_ptr<spdlog::logger>                  pPrevious;
        std::shared_ptr<spdlog::sinks::callback_sink_mt> pSink;
        std::vector<std::string>                         vLines;
        int                                              error_count = 0;
        int                                              warn_count  = 0;
};

} // namespace se_test

#endif // SE_TEST_LOG_CAPTURE_H

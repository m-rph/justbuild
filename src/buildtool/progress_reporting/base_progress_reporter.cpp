#ifndef BOOTSTRAP_BUILD_TOOL

#include "src/buildtool/progress_reporting/base_progress_reporter.hpp"

#include <mutex>

#include <gsl-lite/gsl-lite.hpp>

#include "fmt/core.h"
#include "src/buildtool/common/statistics.hpp"
#include "src/buildtool/logging/logger.hpp"
#include "src/buildtool/progress_reporting/progress.hpp"

auto BaseProgressReporter::Reporter() -> GraphTraverser::progress_reporter_t {
    return [](std::atomic<bool>* done, std::condition_variable* cv) {
        std::mutex m;
        auto const& stats = Statistics::Instance();
        std::unique_lock<std::mutex> lock(m);
        int64_t delay = kStartDelayMillis;
        int total = gsl::narrow<int>(Progress::Instance().OriginMap().size());
        while (not *done) {
            cv->wait_for(lock, std::chrono::milliseconds(delay));
            if (not *done) {
                // Note: order matters; queued has to be queried last
                std::string sample = Progress::Instance().Sample();
                int cached = stats.ActionsCachedCounter();
                int run = stats.ActionsExecutedCounter();
                int queued = stats.ActionsQueuedCounter();
                int active = queued - run - cached;
                std::string now_msg;
                if (active > 0 and !sample.empty()) {
                    auto const& origin_map = Progress::Instance().OriginMap();
                    auto origins = origin_map.find(sample);
                    if (origins != origin_map.end() and
                        !origins->second.empty()) {
                        auto const& origin = origins->second[0];
                        now_msg = fmt::format(" ({}#{}{})",
                                              origin.first.target.ToString(),
                                              origin.second,
                                              active > 1 ? ", ..." : "");
                    }
                    else {
                        now_msg = fmt::format(
                            " ({}{})", sample, active > 1 ? ", ..." : "");
                    }
                }
                constexpr int kOneHundred{100};
                int total_work = total - cached;
                int progress =
                    kOneHundred;  // default if no work has to be done
                if (total_work > 0) {
                    progress = run * kOneHundred / total_work;
                }
                Logger::Log(LogLevel::Progress,
                            "[{:3}%] {} cached, {} run, {} processing{}.",
                            progress,
                            cached,
                            run,
                            active,
                            now_msg);
            }
            delay = delay * kDelayScalingFactorNumerator /
                    kDelayScalingFactorDenominator;
        }
    };
}

#endif

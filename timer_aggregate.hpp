#pragma once

#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <ostream>
#include <string_view>
#include <unordered_map>

enum class AggregateTimerEvent
{
    WaitForReadyTask,
    CreatePack,
    TaskExecution,
    LoadConfig,
    ConnectDatabase,
    ProcessData,
    SaveResults,
    Count // Sentinel: number of events
};

constexpr std::string_view eventName(AggregateTimerEvent event) noexcept
{
    switch (event)
    {
    case AggregateTimerEvent::WaitForReadyTask: return "WaitForReadyTask";
    case AggregateTimerEvent::CreatePack:       return "CreatePack";
    case AggregateTimerEvent::TaskExecution:    return "TaskExecution";
    case AggregateTimerEvent::LoadConfig:       return "LoadConfig";
    case AggregateTimerEvent::ConnectDatabase:  return "ConnectDatabase";
    case AggregateTimerEvent::ProcessData:      return "ProcessData";
    case AggregateTimerEvent::SaveResults:      return "SaveResults";
    case AggregateTimerEvent::Count:            break;
    }

    return "Unknown";
}

class EventTimer
{
public:
    using Clock = std::chrono::steady_clock;

    struct Statistics
    {
        std::uint64_t count = 0;
        double sumMs = 0.0;
        double minMs = std::numeric_limits<double>::max();
        double maxMs = 0.0;

        double averageMs() const noexcept {
            return count == 0 ? 0.0 : sumMs / count;
        }
    };

    // Singleton registry shared by all timers.
    static EventTimer& instance()
    {
        static EventTimer timer;
        return timer;
    }

    class Scoped
    {
    public:
        explicit Scoped(AggregateTimerEvent event) noexcept
            : event_(event), start_(Clock::now())
        {
        }

        ~Scoped() noexcept
        {
            const auto elapsed = Clock::now() - start_;
            const double ms =
                std::chrono::duration<double, std::milli>(
                    elapsed).count();

            EventTimer::instance().record(event_, ms);
        }

        Scoped(const Scoped&) = delete;
        Scoped& operator=(const Scoped&) = delete;
        Scoped(Scoped&&) = delete;
        Scoped& operator=(Scoped&&) = delete;

    private:
        AggregateTimerEvent event_;
        Clock::time_point start_;
    };

    // Record one completed event.
    void record(AggregateTimerEvent event, double durationMs)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& s = stats_[event];
        ++s.count;
        s.sumMs += durationMs;

        if (durationMs < s.minMs)
            s.minMs = durationMs;

        if (durationMs > s.maxMs)
            s.maxMs = durationMs;
    }

    // Print all accumulated statistics.
    void report(std::ostream& out = std::clog) const
    {
        // Take a snapshot under lock; do stream I/O after unlocking.
        std::unordered_map<AggregateTimerEvent, Statistics> snapshot;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = stats_;
        }

        out << std::fixed << std::setfill(' ') << std::setprecision(3);
        out << "\n========== Event Timing Report ==========\n";

        out << std::left
            << std::setw(22) << "Event"
            << std::right
            << std::setw(10) << "Count"
            << std::setw(14) << "Sum (ms)"
            << std::setw(14) << "Avg (ms)"
            << std::setw(14) << "Min (ms)"
            << std::setw(14) << "Max (ms)"
            << '\n';

        for (int i = 0; i < static_cast<int>(AggregateTimerEvent::Count); ++i)
        {
            const auto event = static_cast<AggregateTimerEvent>(i);
            const auto it = snapshot.find(event);

            if (it == snapshot.end() || it->second.count == 0)
                continue;

            const auto& s = it->second;

            out << std::left << std::setw(22) << eventName(event)
                << std::right << std::setw(10) << s.count
                << std::setw(14) << s.sumMs
                << std::setw(14) << s.averageMs()
                << std::setw(14) << s.minMs
                << std::setw(14) << s.maxMs
                << '\n';
        }

        out << "=========================================\n";
    }

    // Clear all accumulated statistics.
    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.clear();
    }

private:
    EventTimer() = default;

    mutable std::mutex mutex_;
    std::unordered_map<AggregateTimerEvent, Statistics> stats_;
};

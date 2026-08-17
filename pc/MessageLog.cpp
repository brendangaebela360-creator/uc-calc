#include "MessageLog.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

MessageLog::MessageLog(const std::string& path)
    : out_(path, std::ios::out | std::ios::app), path_(path)
{
    if (!out_) {
        throw std::runtime_error("Logdatei " + path + " nicht zu oeffnen.");
    }
    note("--- Sitzung gestartet ---");
}

MessageLog::~MessageLog()
{
    if (out_) {
        note("--- Sitzung beendet ---");
    }
}

std::string MessageLog::timestamp()
{
    using namespace std::chrono;

    const auto now  = system_clock::now();
    const auto time = system_clock::to_time_t(now);
    const auto ms   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void MessageLog::log(Direction direction, const std::string& message)
{
    out_ << timestamp() << ' '
         << (direction == Direction::Sent ? "TX" : "RX") << ' '
         << message << '\n';
    out_.flush();
}

void MessageLog::note(const std::string& text)
{
    out_ << timestamp() << " -- " << text << '\n';
    out_.flush();
}

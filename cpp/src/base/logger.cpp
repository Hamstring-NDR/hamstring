#include "hamstring/base/logger.hpp"
#include <map>
#include <mutex>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hamstring {
namespace base {

namespace {
std::map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
std::mutex loggers_mutex_;
} // namespace

std::shared_ptr<spdlog::logger>
Logger::get_logger(const std::string &module_name) {
  std::lock_guard<std::mutex> lock(loggers_mutex_);

  auto it = loggers_.find(module_name);
  if (it != loggers_.end()) {
    return it->second;
  }

  auto logger = create_logger(module_name);
  loggers_[module_name] = logger;
  return logger;
}

void Logger::set_level(const std::string &module_name,
                       spdlog::level::level_enum level) {
  auto logger = get_logger(module_name);
  logger->set_level(level);
}

void Logger::set_global_level(spdlog::level::level_enum level) {
  spdlog::set_level(level);
}

void Logger::initialize(bool debug) {
  auto level = debug ? spdlog::level::debug : spdlog::level::info;
  spdlog::set_level(level);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
}

std::shared_ptr<spdlog::logger> Logger::create_logger(const std::string &name) {
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::trace);

  std::vector<spdlog::sink_ptr> sinks{console_sink};

  auto logger =
      std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
  logger->set_level(spdlog::level::info);

  return logger;
}

} // namespace base
} // namespace hamstring

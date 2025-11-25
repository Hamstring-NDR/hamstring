#pragma once

#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>

namespace hamstring {
namespace base {

class Logger {
public:
  // Get or create a logger for a specific module
  static std::shared_ptr<spdlog::logger>
  get_logger(const std::string &module_name);

  // Set log level for a specific module
  static void set_level(const std::string &module_name,
                        spdlog::level::level_enum level);

  // Set log level for all loggers
  static void set_global_level(spdlog::level::level_enum level);

  // Initialize logging system with configuration
  static void initialize(bool debug = false);

private:
  static std::shared_ptr<spdlog::logger> create_logger(const std::string &name);
};

} // namespace base
} // namespace hamstring

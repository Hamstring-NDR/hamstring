#pragma once

#include <string>

namespace hamstring {
namespace base {

/**
 * @brief ClickHouse stub for monitoring (logs to spdlog instead of database)
 *
 * This is a lightweight stub that provides the ClickHouse interface but
 * logs metrics instead of writing to a database. Useful for testing and
 * when ClickHouse is not available.
 */
class ClickHouseSender {
public:
  ClickHouseSender(const std::string &hostname, int port = 9000,
                   const std::string &database = "default",
                   const std::string &user = "default",
                   const std::string &password = "");
  ~ClickHouseSender();

  // Batch tracking (logged as structured messages)
  void insert_batch_timestamp(const std::string &batch_id,
                              const std::string &stage,
                              const std::string &instance_name,
                              const std::string &status, size_t message_count,
                              bool is_active = true);

  // Logline tracking
  void insert_logline_timestamp(const std::string &logline_id,
                                const std::string &stage,
                                const std::string &status,
                                bool is_active = true);

  // Metrics/fill levels
  void insert_fill_level(const std::string &stage,
                         const std::string &entry_type, size_t entry_count);

  // DGA detections
  void insert_dga_detection(const std::string &domain, double score,
                            const std::string &batch_id,
                            const std::string &src_ip);

  // Generic methods
  void execute(const std::string &query);
  bool ping();

private:
  std::string hostname_;
  int port_;
  std::string database_;
  std::string user_;
  std::string password_;
  bool connected_;
};

} // namespace base
} // namespace hamstring

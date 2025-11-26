#pragma once

#include <clickhouse/client.h>
#include <memory>
#include <string>

namespace hamstring {
namespace base {

/**
 * @brief ClickHouse client for monitoring and logging
 *
 * Provides interface for inserting data into ClickHouse tables.
 * Supports server logs, timestamps, batch tracking, and metrics.
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

  // Server logs (LogServer module)
  void insert_server_log(const std::string &message_id, int64_t timestamp_ms,
                         const std::string &message_text);
  void insert_server_log_timestamp(const std::string &message_id,
                                   const std::string &event,
                                   int64_t event_timestamp_ms);

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
  std::unique_ptr<clickhouse::Client> client_;
};

} // namespace base
} // namespace hamstring

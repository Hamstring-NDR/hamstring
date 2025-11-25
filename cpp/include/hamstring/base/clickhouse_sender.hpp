#pragma once

#include <chrono>
// #include <clickhouse/client.h>  // TODO: Enable when clickhouse-cpp is
// available
#include <memory>
#include <string>
#include <vector>

namespace hamstring {
namespace base {

// ClickHouse batch sender for monitoring data
class ClickHouseSender {
public:
  ClickHouseSender(const std::string &hostname, int port = 9000,
                   const std::string &database = "default",
                   const std::string &user = "default",
                   const std::string &password = "");
  ~ClickHouseSender();

  // Execute a query
  void execute(const std::string &query);

  // Insert data into a table
  void insert(const std::string &table, const std::vector<std::string> &columns,
              const std::vector<std::vector<std::string>> &rows);

  // Check if connection is alive
  bool ping();

private:
  std::string hostname_;
  int port_;
  std::string database_;
  // std::unique_ptr<clickhouse::Client> client_;  // TODO: Enable when
  // clickhouse-cpp is available
};

// Batched ClickHouse sender with timeout
class ClickHouseBatchSender {
public:
  ClickHouseBatchSender(
      std::shared_ptr<ClickHouseSender> sender, const std::string &table,
      const std::vector<std::string> &columns, size_t batch_size = 50,
      std::chrono::seconds batch_timeout = std::chrono::seconds(2));
  ~ClickHouseBatchSender();

  // Add a row to the batch
  void add_row(const std::vector<std::string> &row);

  // Flush the batch
  void flush();

  // Start auto-flush timer
  void start_auto_flush();

  // Stop auto-flush timer
  void stop_auto_flush();

private:
  void check_and_flush();

  std::shared_ptr<ClickHouseSender> sender_;
  std::string table_;
  std::vector<std::string> columns_;
  size_t batch_size_;
  std::chrono::seconds batch_timeout_;
  std::vector<std::vector<std::string>> batch_;
  std::chrono::system_clock::time_point last_flush_;
  bool auto_flush_running_ = false;
};

} // namespace base
} // namespace hamstring

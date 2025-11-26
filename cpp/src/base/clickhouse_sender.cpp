#include "hamstring/base/clickhouse_sender.hpp"
#include "hamstring/base/logger.hpp"
#include <clickhouse/columns/array.h>
#include <clickhouse/columns/date.h>
#include <clickhouse/columns/numeric.h>
#include <clickhouse/columns/string.h>
#include <clickhouse/columns/uuid.h>

namespace hamstring {
namespace base {

ClickHouseSender::ClickHouseSender(const std::string &hostname, int port,
                                   const std::string &database,
                                   const std::string &user,
                                   const std::string &password)
    : hostname_(hostname), port_(port), database_(database), user_(user),
      password_(password), connected_(false) {

  auto logger = Logger::get_logger("clickhouse");

  try {
    // Create ClickHouse client with connection options
    clickhouse::ClientOptions options;
    options.SetHost(hostname_);
    options.SetPort(port_);
    options.SetDefaultDatabase(database_);
    options.SetUser(user_);
    options.SetPassword(password_);
    options.SetPingBeforeQuery(true);

    client_ = std::make_unique<clickhouse::Client>(options);
    connected_ = true;

    logger->info("ClickHouse client connected to {}:{}/{}", hostname_, port_,
                 database_);
  } catch (const std::exception &e) {
    logger->error("Failed to connect to ClickHouse: {}", e.what());
    connected_ = false;
  }
}

ClickHouseSender::~ClickHouseSender() = default;

void ClickHouseSender::insert_batch_timestamp(const std::string &batch_id,
                                              const std::string &stage,
                                              const std::string &instance_name,
                                              const std::string &status,
                                              size_t message_count,
                                              bool is_active) {
  auto logger = Logger::get_logger("clickhouse.metrics");
  logger->debug("BATCH_TIMESTAMP: batch_id={}, stage={}, instance={}, "
                "status={}, count={}, active={}",
                batch_id, stage, instance_name, status, message_count,
                is_active);
}

void ClickHouseSender::insert_logline_timestamp(const std::string &logline_id,
                                                const std::string &stage,
                                                const std::string &status,
                                                bool is_active) {
  auto logger = Logger::get_logger("clickhouse.metrics");
  logger->trace(
      "LOGLINE_TIMESTAMP: logline_id={}, stage={}, status={}, active={}",
      logline_id, stage, status, is_active);
}

void ClickHouseSender::insert_fill_level(const std::string &stage,
                                         const std::string &entry_type,
                                         size_t entry_count) {
  auto logger = Logger::get_logger("clickhouse.metrics");
  logger->debug("FILL_LEVEL: stage={}, type={}, count={}", stage, entry_type,
                entry_count);
}

void ClickHouseSender::insert_dga_detection(const std::string &domain,
                                            double score,
                                            const std::string &batch_id,
                                            const std::string &src_ip) {
  auto logger = Logger::get_logger("clickhouse.detections");
  logger->info("DGA_DETECTION: domain={}, score={:.4f}, batch={}, ip={}",
               domain, score, batch_id, src_ip);
}

void ClickHouseSender::execute(const std::string &query) {
  auto logger = Logger::get_logger("clickhouse");
  logger->debug("EXECUTE: {}", query);
}

bool ClickHouseSender::ping() {
  if (!connected_ || !client_) {
    return false;
  }

  try {
    client_->Execute("SELECT 1");
    return true;
  } catch (const std::exception &e) {
    auto logger = Logger::get_logger("clickhouse");
    logger->error("ClickHouse ping failed: {}", e.what());
    return false;
  }
}

void ClickHouseSender::insert_server_log(const std::string &message_id,
                                         int64_t timestamp_ms,
                                         const std::string &message_text) {
  if (!connected_ || !client_) {
    auto logger = Logger::get_logger("clickhouse");
    logger->warn("ClickHouse not connected, skipping server_log insert");
    return;
  }

  try {
    // Create block with columns
    clickhouse::Block block;

    // message_id as String (ClickHouse will convert to UUID)
    // Using String instead of UUID column to avoid boost UUID parsing
    // complexity
    auto col_message_id = std::make_shared<clickhouse::ColumnString>();
    col_message_id->Append(message_id);

    // timestamp_in as DateTime64(6) - represented as milliseconds since epoch
    auto col_timestamp = std::make_shared<clickhouse::ColumnDateTime64>(6);
    col_timestamp->Append(timestamp_ms * 1000); // Convert ms to microseconds

    // message_text as String
    auto col_message = std::make_shared<clickhouse::ColumnString>();
    col_message->Append(message_text);

    // Add columns to block
    block.AppendColumn("message_id", col_message_id);
    block.AppendColumn("timestamp_in", col_timestamp);
    block.AppendColumn("message_text", col_message);

    // Insert block
    client_->Insert("server_logs", block);

  } catch (const std::exception &e) {
    auto logger = Logger::get_logger("clickhouse");
    logger->error("Failed to insert server_log: {}", e.what());
  }
}

void ClickHouseSender::insert_server_log_timestamp(
    const std::string &message_id, const std::string &event,
    int64_t event_timestamp_ms) {
  if (!connected_ || !client_) {
    auto logger = Logger::get_logger("clickhouse");
    logger->warn(
        "ClickHouse not connected, skipping server_log_timestamp insert");
    return;
  }

  try {
    // Create block with columns
    clickhouse::Block block;

    // message_id as String (ClickHouse will convert to UUID)
    auto col_message_id = std::make_shared<clickhouse::ColumnString>();
    col_message_id->Append(message_id);

    // event as String
    auto col_event = std::make_shared<clickhouse::ColumnString>();
    col_event->Append(event);

    // event_timestamp as DateTime64(6)
    auto col_timestamp = std::make_shared<clickhouse::ColumnDateTime64>(6);
    col_timestamp->Append(event_timestamp_ms *
                          1000); // Convert ms to microseconds

    // Add columns to block
    block.AppendColumn("message_id", col_message_id);
    block.AppendColumn("event", col_event);
    block.AppendColumn("event_timestamp", col_timestamp);

    // Insert block
    client_->Insert("server_logs_timestamps", block);

  } catch (const std::exception &e) {
    auto logger = Logger::get_logger("clickhouse");
    logger->error("Failed to insert server_log_timestamp: {}", e.what());
  }
}

} // namespace base
} // namespace hamstring

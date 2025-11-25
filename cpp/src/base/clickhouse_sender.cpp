#include "hamstring/base/clickhouse_sender.hpp"
#include "hamstring/base/logger.hpp"

namespace hamstring {
namespace base {

ClickHouseSender::ClickHouseSender(const std::string &hostname, int port,
                                   const std::string &database,
                                   const std::string &user,
                                   const std::string &password)
    : hostname_(hostname), port_(port), database_(database), user_(user),
      password_(password), connected_(false) {

  auto logger = Logger::get_logger("clickhouse");
  logger->info("ClickHouse stub initialized (monitoring via logs only)");
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
  return false; // Stub always returns false
}

} // namespace base
} // namespace hamstring

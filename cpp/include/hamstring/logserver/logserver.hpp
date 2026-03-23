#pragma once

#include "hamstring/base/clickhouse_sender.hpp"
#include "hamstring/base/kafka_handler.hpp"
#include "hamstring/base/logger.hpp"
#include "hamstring/config/config.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace hamstring {
namespace logserver {

/**
 * @brief LogServer - Entry point for log data into the pipeline
 *
 * The LogServer consumes log messages from Kafka topics, stores them in
 * ClickHouse for monitoring, and forwards them to the appropriate collector
 * topics based on protocol configuration.
 *
 * Features:
 * - Consumes from multiple Kafka input topics
 * - Produces to multiple collector topics
 * - Logs all messages and timestamps to ClickHouse
 * - Async processing for high throughput
 * - Graceful shutdown support
 */
class LogServer {
public:
  /**
   * @brief Construct a LogServer instance
   *
   * @param consume_topic Kafka topic to consume from
   * @param produce_topics List of Kafka topics to produce to
   * @param clickhouse ClickHouse sender for monitoring
   * @param bootstrap_servers Comma-separated Kafka broker addresses
   * @param group_id Kafka consumer group ID
   */
  LogServer(const std::string &consume_topic,
            const std::vector<std::string> &produce_topics,
            std::shared_ptr<base::ClickHouseSender> clickhouse,
            const std::string &bootstrap_servers, const std::string &group_id);

  ~LogServer();

  /**
   * @brief Start the LogServer
   *
   * Begins consuming messages from Kafka and processing them.
   * This method blocks until stop() is called.
   */
  void start();

  /**
   * @brief Stop the LogServer
   *
   * Gracefully shuts down the server, finishing any in-flight messages.
   */
  void stop();

  /**
   * @brief Check if server is running
   */
  bool is_running() const { return running_; }

private:
  /**
   * @brief Send a message to all producer topics
   *
   * @param message_id UUID of the message
   * @param message Message content
   */
  void send(const std::string &message_id, const std::string &message);

  /**
   * @brief Main message fetching loop
   *
   * Continuously fetches messages from Kafka and processes them.
   */
  void fetch_from_kafka();

  /**
   * @brief Log message to ClickHouse
   *
   * @param message_id UUID of the message
   * @param message Message content
   */
  void log_message(const std::string &message_id, const std::string &message);

  /**
   * @brief Log timestamp event to ClickHouse
   *
   * @param message_id UUID of the message
   * @param event Event type (timestamp_in, timestamp_out)
   */
  void log_timestamp(const std::string &message_id, const std::string &event);

  // Configuration
  std::string consume_topic_;
  std::vector<std::string> produce_topics_;

  // Kafka handlers
  std::unique_ptr<base::KafkaConsumeHandler> consumer_;
  std::vector<std::unique_ptr<base::KafkaProduceHandler>> producers_;

  // ClickHouse for monitoring
  std::shared_ptr<base::ClickHouseSender> clickhouse_;

  // Logger
  std::shared_ptr<spdlog::logger> logger_;

  // Runtime state
  std::atomic<bool> running_;
  std::thread worker_thread_;
};

/**
 * @brief Create and start LogServer instances based on configuration
 *
 * Creates one LogServer instance per protocol defined in the configuration.
 * Each server consumes from its protocol-specific topic and produces to
 * collector topics that handle that protocol.
 *
 * @param config Application configuration
 * @return Vector of LogServer instances
 */
std::vector<std::shared_ptr<LogServer>>
create_logservers(std::shared_ptr<config::Config> config);

} // namespace logserver
} // namespace hamstring

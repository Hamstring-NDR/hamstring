#include "hamstring/logserver/logserver.hpp"
#include "hamstring/base/utils.hpp"
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hamstring {
namespace logserver {

LogServer::LogServer(const std::string &consume_topic,
                     const std::vector<std::string> &produce_topics,
                     std::shared_ptr<base::ClickHouseSender> clickhouse,
                     const std::string &bootstrap_servers,
                     const std::string &group_id)
    : consume_topic_(consume_topic), produce_topics_(produce_topics),
      clickhouse_(clickhouse), running_(false) {
  logger_ = base::Logger::get_logger("logserver");

  // Create Kafka consumer
  consumer_ = std::make_unique<base::KafkaConsumeHandler>(
      bootstrap_servers, group_id, std::vector<std::string>{consume_topic_});

  // Create Kafka producers for each output topic
  for (const auto &topic : produce_topics_) {
    auto producer =
        std::make_unique<base::KafkaProduceHandler>(bootstrap_servers, topic);
    producers_.push_back(std::move(producer));
  }

  logger_->info("LogServer created for topic: {}", consume_topic_);
  logger_->info("Will produce to {} topics", produce_topics_.size());
}

LogServer::~LogServer() { stop(); }

void LogServer::start() {
  if (running_) {
    logger_->warn("LogServer already running");
    return;
  }

  running_ = true;

  logger_->info("LogServer started:");
  logger_->info("  ⤷ receiving on Kafka topic '{}'", consume_topic_);
  logger_->info("  ⤷ sending on Kafka topics:");
  for (const auto &topic : produce_topics_) {
    logger_->info("      - {}", topic);
  }

  // Start worker thread for Kafka consumption
  worker_thread_ = std::thread(&LogServer::fetch_from_kafka, this);
}

void LogServer::stop() {
  if (!running_) {
    return;
  }

  logger_->info("Stopping LogServer...");
  running_ = false;

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  // Flush all producers
  for (auto &producer : producers_) {
    producer->flush();
  }

  logger_->info("LogServer stopped");
}

void LogServer::send(const std::string &message_id,
                     const std::string &message) {
  // Send to all producer topics
  for (size_t i = 0; i < producers_.size(); ++i) {
    try {
      producers_[i]->send(message_id, message);
      logger_->trace("Sent message {} to topic {}", message_id,
                     produce_topics_[i]);
    } catch (const std::exception &e) {
      logger_->error("Failed to send to topic {}: {}", produce_topics_[i],
                     e.what());
    }
  }

  // Log timestamp
  log_timestamp(message_id, "timestamp_out");
}

void LogServer::fetch_from_kafka() {
  logger_->info("Starting Kafka consumer loop");

  while (running_) {
    try {
      // Poll for messages with 1 second timeout
      consumer_->poll(
          [this](const std::string &topic, const std::string &key,
                 const std::string &value, int64_t timestamp) {
            if (!running_)
              return;

            // Generate message ID (or use key if provided)
            std::string message_id =
                key.empty() ? base::utils::generate_uuid() : key;

            logger_->debug("From Kafka ({}): {}", topic, value.substr(0, 100));

            // Log timestamp in
            log_timestamp(message_id, "timestamp_in");

            // Log to ClickHouse
            log_message(message_id, value);

            // Forward to collectors
            send(message_id, value);

            // Commit the offset
            consumer_->commit();
          },
          1000); // 1 second timeout

    } catch (const std::exception &e) {
      logger_->error("Error in consumer loop: {}", e.what());
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  logger_->info("Kafka consumer loop stopped");
}

void LogServer::log_message(const std::string &message_id,
                            const std::string &message) {
  // Log the message to ClickHouse server_logs table
  auto now = std::chrono::system_clock::now();
  auto timestamp_ms = base::utils::timestamp_to_ms(now);

  logger_->trace("Logging message {} to ClickHouse", message_id);

  // TODO: Implement ClickHouse insert when clickhouse-cpp is available
  // For now, just log
  // clickhouse_->insert("server_logs",
  //     {"message_id", "timestamp_in", "message_text"},
  //     {{message_id, std::to_string(timestamp_ms), message}});
}

void LogServer::log_timestamp(const std::string &message_id,
                              const std::string &event) {
  // Log timestamp event to ClickHouse server_logs_timestamps table
  auto now = std::chrono::system_clock::now();
  auto timestamp_ms = base::utils::timestamp_to_ms(now);

  logger_->trace("Logging timestamp {} event for message {}", event,
                 message_id);

  // TODO: Implement ClickHouse insert when clickhouse-cpp is available
  // clickhouse_->insert("server_logs_timestamps",
  //     {"message_id", "event", "event_timestamp"},
  //     {{message_id, event, std::to_string(timestamp_ms)}});
}

std::vector<std::shared_ptr<LogServer>>
create_logservers(std::shared_ptr<config::Config> config) {
  std::vector<std::shared_ptr<LogServer>> servers;
  auto logger = base::Logger::get_logger("logserver.factory");

  // Get topic prefixes from config
  auto &env = config->environment;
  std::string consume_prefix = env.kafka_topics_prefix["logserver_in"];
  std::string produce_prefix =
      env.kafka_topics_prefix["logserver_to_collector"];

  // Build bootstrap servers string
  std::vector<std::string> broker_addresses;
  for (const auto &broker : env.kafka_brokers) {
    broker_addresses.push_back(broker.hostname + ":" +
                               std::to_string(broker.internal_port));
  }
  std::string bootstrap_servers;
  for (size_t i = 0; i < broker_addresses.size(); ++i) {
    if (i > 0)
      bootstrap_servers += ",";
    bootstrap_servers += broker_addresses[i];
  }

  // Create ClickHouse sender for monitoring
  auto clickhouse = std::make_shared<base::ClickHouseSender>(
      env.clickhouse_hostname, 9000, "hamstring", "default", "");

  // Get unique protocols from collectors
  std::set<std::string> protocols;
  for (const auto &collector : config->pipeline.collectors) {
    protocols.insert(collector.protocol_base);
  }

  logger->info("Creating LogServers for {} protocols", protocols.size());
  logger->info("Kafka brokers: {}", bootstrap_servers);

  // Create one LogServer per protocol
  for (const auto &protocol : protocols) {
    std::string consume_topic = consume_prefix + "-" + protocol;
    std::string group_id = "logserver-" + protocol;

    // Find all collectors for this protocol
    std::vector<std::string> produce_topics;
    for (const auto &collector : config->pipeline.collectors) {
      if (collector.protocol_base == protocol) {
        std::string topic = produce_prefix + "-" + collector.name;
        produce_topics.push_back(topic);
      }
    }

    logger->info("Creating LogServer for protocol '{}' -> {} collectors",
                 protocol, produce_topics.size());

    auto server = std::make_shared<LogServer>(
        consume_topic, produce_topics, clickhouse, bootstrap_servers, group_id);
    servers.push_back(server);
  }

  return servers;
}

} // namespace logserver
} // namespace hamstring

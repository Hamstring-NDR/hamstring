#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <librdkafka/rdkafkacpp.h>
#include <memory>
#include <string>
#include <vector>

namespace hamstring {
namespace base {

// Kafka message callback
using KafkaMessageCallback =
    std::function<void(const std::string &topic, const std::string &key,
                       const std::string &value, int64_t timestamp)>;

// Base Kafka handler
class KafkaHandler {
public:
  virtual ~KafkaHandler() = default;

  // Get Kafka configuration
  static std::unique_ptr<RdKafka::Conf>
  create_config(const std::string &bootstrap_servers,
                const std::string &group_id = "");

protected:
  std::string bootstrap_servers_;
  std::unique_ptr<RdKafka::Conf> conf_;
};

// Kafka producer
class KafkaProduceHandler : public KafkaHandler {
public:
  KafkaProduceHandler(const std::string &bootstrap_servers,
                      const std::string &topic);
  ~KafkaProduceHandler();

  // Send a message
  bool send(const std::string &key, const std::string &value);

  // Send a message with timestamp
  bool send(const std::string &key, const std::string &value,
            int64_t timestamp);

  // Flush pending messages
  void flush(int timeout_ms = 10000);

private:
  std::string topic_;
  std::unique_ptr<RdKafka::Producer> producer_;
  std::unique_ptr<RdKafka::Topic> topic_handle_;
};

// Kafka consumer
class KafkaConsumeHandler : public KafkaHandler {
public:
  KafkaConsumeHandler(const std::string &bootstrap_servers,
                      const std::string &group_id,
                      const std::vector<std::string> &topics);
  ~KafkaConsumeHandler();

  // Poll for messages (blocking)
  void poll(KafkaMessageCallback callback, int timeout_ms = 1000);

  // Start consuming in background
  void start_async(boost::asio::io_context &io_context,
                   KafkaMessageCallback callback);

  // Stop consuming
  void stop();

  // Commit offsets
  void commit();

private:
  std::vector<std::string> topics_;
  std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
  bool running_ = false;
};

// Exactly-once Kafka handler
class ExactlyOnceKafkaHandler {
public:
  ExactlyOnceKafkaHandler(const std::string &bootstrap_servers,
                          const std::string &consumer_group_id,
                          const std::vector<std::string> &consume_topics,
                          const std::string &produce_topic);
  ~ExactlyOnceKafkaHandler();

  // Process messages with exactly-once semantics
  void
  process(std::function<std::string(const std::string &, const std::string &)>
              transform_fn,
          int timeout_ms = 1000);

  // Start processing in background
  void start_async(
      boost::asio::io_context &io_context,
      std::function<std::string(const std::string &, const std::string &)>
          transform_fn);

  // Stop processing
  void stop();

private:
  std::unique_ptr<KafkaConsumeHandler> consumer_;
  std::unique_ptr<KafkaProduceHandler> producer_;
  bool running_ = false;
};

} // namespace base
} // namespace hamstring

#include "hamstring/base/kafka_handler.hpp"
#include "hamstring/base/logger.hpp"
#include <stdexcept>

namespace hamstring {
namespace base {

// ============================================================================
// KafkaHandler Base Implementation
// ============================================================================

std::unique_ptr<RdKafka::Conf>
KafkaHandler::create_config(const std::string &bootstrap_servers,
                            const std::string &group_id) {

  std::string errstr;
  auto conf = std::unique_ptr<RdKafka::Conf>(
      RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

  if (conf->set("bootstrap.servers", bootstrap_servers, errstr) !=
      RdKafka::Conf::CONF_OK) {
    throw std::runtime_error("Failed to set bootstrap.servers: " + errstr);
  }

  if (!group_id.empty()) {
    if (conf->set("group.id", group_id, errstr) != RdKafka::Conf::CONF_OK) {
      throw std::runtime_error("Failed to set group.id: " + errstr);
    }
  }

  return conf;
}

// ============================================================================
// KafkaProduceHandler Implementation
// ============================================================================

KafkaProduceHandler::KafkaProduceHandler(const std::string &bootstrap_servers,
                                         const std::string &topic)
    : topic_(topic) {

  bootstrap_servers_ = bootstrap_servers;

  std::string errstr;
  conf_ = create_config(bootstrap_servers);

  // Producer-specific settings
  conf_->set("enable.idempotence", "false", errstr);
  conf_->set("acks", "1", errstr);
  conf_->set("message.max.bytes", "1000000000", errstr);

  // Create producer
  producer_ = std::unique_ptr<RdKafka::Producer>(
      RdKafka::Producer::create(conf_.get(), errstr));

  if (!producer_) {
    throw std::runtime_error("Failed to create Kafka producer: " + errstr);
  }

  auto logger = Logger::get_logger("kafka.producer");
  logger->info("Kafka producer created for topic '{}'", topic_);
}

KafkaProduceHandler::~KafkaProduceHandler() {
  if (producer_) {
    producer_->flush(10000);
  }
}

bool KafkaProduceHandler::send(const std::string &key,
                               const std::string &value) {
  return send(key, value, 0); // timestamp=0 means current time
}

bool KafkaProduceHandler::send(const std::string &key, const std::string &value,
                               int64_t timestamp) {
  if (value.empty()) {
    return true;
  }

  RdKafka::ErrorCode err = producer_->produce(
      topic_, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
      const_cast<char *>(value.data()), value.size(),
      key.empty() ? nullptr : key.data(), key.size(), timestamp,
      nullptr // headers
  );

  if (err != RdKafka::ERR_NO_ERROR) {
    auto logger = Logger::get_logger("kafka.producer");
    logger->error("Failed to produce message: {}", RdKafka::err2str(err));
    return false;
  }

  producer_->poll(0); // Handle delivery reports
  return true;
}

void KafkaProduceHandler::flush(int timeout_ms) {
  if (producer_) {
    producer_->flush(timeout_ms);
  }
}

// ============================================================================
// KafkaConsumeHandler Implementation
// ============================================================================

KafkaConsumeHandler::KafkaConsumeHandler(const std::string &bootstrap_servers,
                                         const std::string &group_id,
                                         const std::vector<std::string> &topics)
    : topics_(topics) {

  bootstrap_servers_ = bootstrap_servers;

  std::string errstr;
  conf_ = create_config(bootstrap_servers, group_id);

  // Consumer-specific settings
  conf_->set("enable.auto.commit", "false", errstr);
  conf_->set("auto.offset.reset", "earliest", errstr);
  conf_->set("enable.partition.eof", "true", errstr);

  // Create consumer
  consumer_ = std::unique_ptr<RdKafka::KafkaConsumer>(
      RdKafka::KafkaConsumer::create(conf_.get(), errstr));

  if (!consumer_) {
    throw std::runtime_error("Failed to create Kafka consumer: " + errstr);
  }

  // Subscribe to topics
  RdKafka::ErrorCode err = consumer_->subscribe(topics_);
  if (err != RdKafka::ERR_NO_ERROR) {
    throw std::runtime_error("Failed to subscribe to topics: " +
                             RdKafka::err2str(err));
  }

  auto logger = Logger::get_logger("kafka.consumer");
  logger->info("Kafka consumer created for {} topics (group: {})",
               topics_.size(), group_id);
}

KafkaConsumeHandler::~KafkaConsumeHandler() {
  stop();
  if (consumer_) {
    consumer_->close();
  }
}

void KafkaConsumeHandler::poll(KafkaMessageCallback callback, int timeout_ms) {
  RdKafka::Message *msg = consumer_->consume(timeout_ms);

  if (!msg) {
    return;
  }

  RdKafka::ErrorCode err = msg->err();

  if (err == RdKafka::ERR__TIMED_OUT || err == RdKafka::ERR__PARTITION_EOF) {
    delete msg;
    return;
  }

  if (err != RdKafka::ERR_NO_ERROR) {
    auto logger = Logger::get_logger("kafka.consumer");
    logger->error("Kafka consume error: {}", msg->errstr());
    delete msg;
    return;
  }

  // Extract message data
  std::string topic = msg->topic_name();
  std::string key;
  if (msg->key()) {
    key =
        std::string(reinterpret_cast<const char *>(msg->key()), msg->key_len());
  }

  std::string value;
  if (msg->payload()) {
    value = std::string(static_cast<const char *>(msg->payload()), msg->len());
  }

  int64_t timestamp = msg->timestamp().timestamp;

  delete msg;

  // Call the callback
  if (callback) {
    callback(topic, key, value, timestamp);
  }
}

void KafkaConsumeHandler::start_async(boost::asio::io_context &io_context,
                                      KafkaMessageCallback callback) {
  running_ = true;

  // Note: This is a simple placeholder. For production, use boost::asio::post
  // with a proper work guard to keep the io_context running
  // TODO: Implement proper async polling with boost::asio
}

void KafkaConsumeHandler::stop() { running_ = false; }

void KafkaConsumeHandler::commit() {
  if (consumer_) {
    consumer_->commitSync();
  }
}

// ============================================================================
// ExactlyOnceKafkaHandler Implementation
// ============================================================================

ExactlyOnceKafkaHandler::ExactlyOnceKafkaHandler(
    const std::string &bootstrap_servers, const std::string &consumer_group_id,
    const std::vector<std::string> &consume_topics,
    const std::string &produce_topic) {

  consumer_ = std::make_unique<KafkaConsumeHandler>(
      bootstrap_servers, consumer_group_id, consume_topics);

  producer_ =
      std::make_unique<KafkaProduceHandler>(bootstrap_servers, produce_topic);
}

ExactlyOnceKafkaHandler::~ExactlyOnceKafkaHandler() { stop(); }

void ExactlyOnceKafkaHandler::process(
    std::function<std::string(const std::string &, const std::string &)>
        transform_fn,
    int timeout_ms) {

  consumer_->poll(
      [this, transform_fn](const std::string &topic, const std::string &key,
                           const std::string &value, int64_t timestamp) {
        // Transform the message
        std::string transformed = transform_fn(key, value);

        // Send to output topic
        if (!transformed.empty()) {
          producer_->send(key, transformed, timestamp);
        }

        // Commit the offset
        consumer_->commit();
      },
      timeout_ms);
}

void ExactlyOnceKafkaHandler::start_async(
    boost::asio::io_context &io_context,
    std::function<std::string(const std::string &, const std::string &)>
        transform_fn) {

  running_ = true;

  consumer_->start_async(
      io_context,
      [this, transform_fn](const std::string &topic, const std::string &key,
                           const std::string &value, int64_t timestamp) {
        if (!running_)
          return;

        std::string transformed = transform_fn(key, value);

        if (!transformed.empty()) {
          producer_->send(key, transformed, timestamp);
        }

        consumer_->commit();
      });
}

void ExactlyOnceKafkaHandler::stop() {
  running_ = false;
  if (consumer_) {
    consumer_->stop();
  }
  if (producer_) {
    producer_->flush();
  }
}

} // namespace base
} // namespace hamstring

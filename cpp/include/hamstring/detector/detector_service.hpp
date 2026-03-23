```cpp
#pragma once

#include "hamstring/base/clickhouse_sender.hpp"
#include "hamstring/base/data_classes.hpp"
#include "hamstring/base/kafka_handler.hpp"
#include "hamstring/base/logger.hpp"
#include "hamstring/config/config.hpp"
#include "hamstring/detector/detector.hpp"
#include <atomic>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <vector>

    namespace hamstring {
  namespace detector {

  class DetectorService {
  public:
    DetectorService(const std::string &name, const std::string &consume_topic,
                    const std::string &model_path, double threshold,
                    std::shared_ptr<config::Config> config,
                    const std::string &bootstrap_servers,
                    const std::string &group_id);
    ~DetectorService();

    void start();
    void stop();
    bool is_running() const { return running_; }

    struct Stats {
      uint64_t batches_consumed = 0;
      uint64_t domains_scanned = 0;
      uint64_t domains_detected = 0;
    };
    Stats get_stats() const;

  private:
    void consume_loop();
    void process_batch(const base::Batch &batch);

    std::string name_;
    std::string consume_topic_;
    std::string model_path_;
    double threshold_;
    std::shared_ptr<config::Config> config_;

    std::shared_ptr<spdlog::logger> logger_;
    std::unique_ptr<base::KafkaConsumeHandler> consumer_;
    std::shared_ptr<base::ClickHouseSender> clickhouse_;

    // The core detector logic
    Detector detector_;

    std::atomic<bool> running_{false};
    std::thread worker_thread_;

    // Stats
    std::atomic<uint64_t> batches_consumed_{0};
    std::atomic<uint64_t> domains_scanned_{0};
    std::atomic<uint64_t> domains_detected_{0};
  };

  // Factory function
  std::vector<std::shared_ptr<DetectorService>>
  create_detector_services(std::shared_ptr<config::Config> config);

  } // namespace detector
} // namespace hamstring

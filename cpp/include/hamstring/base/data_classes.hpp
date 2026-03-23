#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace hamstring {
namespace base {

// Forward declarations
class FieldValidator;

// LogLine represents a validated log entry
class LogLine {
public:
  std::string logline_id;
  std::string batch_id;
  std::map<std::string, std::string> fields;
  std::chrono::system_clock::time_point timestamp;

  // Serialize to JSON string
  std::string to_json() const;

  // Deserialize from JSON string
  static std::shared_ptr<LogLine> from_json(const std::string &json_str);

  // Get field value
  std::optional<std::string> get_field(const std::string &name) const;

  // Set field value
  void set_field(const std::string &name, const std::string &value);
};

// Batch represents a collection of log lines grouped by subnet
class Batch {
public:
  std::string batch_id;
  std::string subnet_id;
  std::string collector_name;
  std::vector<std::shared_ptr<LogLine>> loglines;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point timestamp_in;

  // Serialize to JSON string
  std::string to_json() const;

  // Deserialize from JSON string
  static std::shared_ptr<Batch> from_json(const std::string &json_str);

  // Add a log line to the batch
  void add_logline(std::shared_ptr<LogLine> logline);

  // Get number of log lines
  size_t size() const { return loglines.size(); }

  // Check if batch is empty
  bool empty() const { return loglines.empty(); }
};

// Warning represents a detected threat
class Warning {
public:
  std::string warning_id;
  std::string batch_id;
  std::string src_ip;
  std::string domain_name;
  double score;
  double threshold;
  std::chrono::system_clock::time_point timestamp;
  std::map<std::string, std::string> metadata;

  // Serialize to JSON string
  std::string to_json() const;

  // Deserialize from JSON string
  static std::shared_ptr<Warning> from_json(const std::string &json_str);
};

// Base class for field validators
class FieldValidator {
public:
  virtual ~FieldValidator() = default;

  // Validate a field value
  virtual bool validate(const std::string &value) const = 0;

  // Get field name
  virtual std::string get_name() const = 0;
};

// RegEx field validator
class RegExValidator : public FieldValidator {
public:
  RegExValidator(const std::string &name, const std::string &pattern);

  bool validate(const std::string &value) const override;
  std::string get_name() const override { return name_; }

private:
  std::string name_;
  std::regex pattern_;
};

// Timestamp field validator
class TimestampValidator : public FieldValidator {
public:
  TimestampValidator(const std::string &name, const std::string &format);

  bool validate(const std::string &value) const override;
  std::string get_name() const override { return name_; }

  // Parse timestamp to time_point
  std::chrono::system_clock::time_point parse(const std::string &value) const;

private:
  std::string name_;
  std::string format_;
};

// IP Address field validator
class IpAddressValidator : public FieldValidator {
public:
  explicit IpAddressValidator(const std::string &name);

  bool validate(const std::string &value) const override;
  std::string get_name() const override { return name_; }

  // Check if IPv4
  static bool is_ipv4(const std::string &value);

  // Check if IPv6
  static bool is_ipv6(const std::string &value);

private:
  std::string name_;
};

// ListItem field validator
class ListItemValidator : public FieldValidator {
public:
  ListItemValidator(const std::string &name,
                    const std::vector<std::string> &allowed_list,
                    const std::vector<std::string> &relevant_list);

  bool validate(const std::string &value) const override;
  std::string get_name() const override { return name_; }

  // Check if value is relevant
  bool is_relevant(const std::string &value) const;

private:
  std::string name_;
  std::vector<std::string> allowed_list_;
  std::vector<std::string> relevant_list_;
};

} // namespace base
} // namespace hamstring

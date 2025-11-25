#include "hamstring/base/data_classes.hpp"
#include "hamstring/base/utils.hpp"
#include <arpa/inet.h>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace hamstring {
namespace base {

// ============================================================================
// LogLine Implementation
// ============================================================================

std::string LogLine::to_json() const {
  json j;
  j["logline_id"] = logline_id;
  j["batch_id"] = batch_id;
  j["fields"] = fields;
  j["timestamp"] = utils::timestamp_to_ms(timestamp);
  return j.dump();
}

std::shared_ptr<LogLine> LogLine::from_json(const std::string &json_str) {
  auto j = json::parse(json_str);
  auto logline = std::make_shared<LogLine>();

  logline->logline_id = j["logline_id"];
  logline->batch_id = j.value("batch_id", "");
  logline->fields = j["fields"].get<std::map<std::string, std::string>>();
  logline->timestamp = utils::ms_to_timestamp(j["timestamp"]);

  return logline;
}

std::optional<std::string> LogLine::get_field(const std::string &name) const {
  auto it = fields.find(name);
  if (it != fields.end()) {
    return it->second;
  }
  return std::nullopt;
}

void LogLine::set_field(const std::string &name, const std::string &value) {
  fields[name] = value;
}

// ============================================================================
// Batch Implementation
// ============================================================================

std::string Batch::to_json() const {
  json j;
  j["batch_id"] = batch_id;
  j["subnet_id"] = subnet_id;
  j["collector_name"] = collector_name;
  j["created_at"] = utils::timestamp_to_ms(created_at);
  j["timestamp_in"] = utils::timestamp_to_ms(timestamp_in);

  json loglines_json = json::array();
  for (const auto &logline : loglines) {
    loglines_json.push_back(json::parse(logline->to_json()));
  }
  j["loglines"] = loglines_json;

  return j.dump();
}

std::shared_ptr<Batch> Batch::from_json(const std::string &json_str) {
  auto j = json::parse(json_str);
  auto batch = std::make_shared<Batch>();

  batch->batch_id = j["batch_id"];
  batch->subnet_id = j["subnet_id"];
  batch->collector_name = j["collector_name"];
  batch->created_at = utils::ms_to_timestamp(j["created_at"]);
  batch->timestamp_in = utils::ms_to_timestamp(j["timestamp_in"]);

  for (const auto &logline_json : j["loglines"]) {
    batch->loglines.push_back(LogLine::from_json(logline_json.dump()));
  }

  return batch;
}

void Batch::add_logline(std::shared_ptr<LogLine> logline) {
  loglines.push_back(logline);
}

// ============================================================================
// Warning Implementation
// ============================================================================

std::string Warning::to_json() const {
  json j;
  j["warning_id"] = warning_id;
  j["batch_id"] = batch_id;
  j["src_ip"] = src_ip;
  j["domain_name"] = domain_name;
  j["score"] = score;
  j["threshold"] = threshold;
  j["timestamp"] = utils::timestamp_to_ms(timestamp);
  j["metadata"] = metadata;

  return j.dump();
}

std::shared_ptr<Warning> Warning::from_json(const std::string &json_str) {
  auto j = json::parse(json_str);
  auto warning = std::make_shared<Warning>();

  warning->warning_id = j["warning_id"];
  warning->batch_id = j["batch_id"];
  warning->src_ip = j["src_ip"];
  warning->domain_name = j["domain_name"];
  warning->score = j["score"];
  warning->threshold = j["threshold"];
  warning->timestamp = utils::ms_to_timestamp(j["timestamp"]);
  warning->metadata = j["metadata"].get<std::map<std::string, std::string>>();

  return warning;
}

// ============================================================================
// RegExValidator Implementation
// ============================================================================

RegExValidator::RegExValidator(const std::string &name,
                               const std::string &pattern)
    : name_(name), pattern_(pattern) {}

bool RegExValidator::validate(const std::string &value) const {
  return std::regex_match(value, pattern_);
}

// ============================================================================
// TimestampValidator Implementation
// ============================================================================

TimestampValidator::TimestampValidator(const std::string &name,
                                       const std::string &format)
    : name_(name), format_(format) {}

bool TimestampValidator::validate(const std::string &value) const {
  try {
    parse(value);
    return true;
  } catch (...) {
    return false;
  }
}

std::chrono::system_clock::time_point
TimestampValidator::parse(const std::string &value) const {
  return utils::parse_timestamp(value, format_);
}

// ============================================================================
// IpAddressValidator Implementation
// ============================================================================

IpAddressValidator::IpAddressValidator(const std::string &name) : name_(name) {}

bool IpAddressValidator::validate(const std::string &value) const {
  return is_ipv4(value) || is_ipv6(value);
}

bool IpAddressValidator::is_ipv4(const std::string &value) {
  return utils::is_valid_ipv4(value);
}

bool IpAddressValidator::is_ipv6(const std::string &value) {
  return utils::is_valid_ipv6(value);
}

// ============================================================================
// ListItemValidator Implementation
// ============================================================================

ListItemValidator::ListItemValidator(
    const std::string &name, const std::vector<std::string> &allowed_list,
    const std::vector<std::string> &relevant_list)
    : name_(name), allowed_list_(allowed_list), relevant_list_(relevant_list) {}

bool ListItemValidator::validate(const std::string &value) const {
  return std::find(allowed_list_.begin(), allowed_list_.end(), value) !=
         allowed_list_.end();
}

bool ListItemValidator::is_relevant(const std::string &value) const {
  return std::find(relevant_list_.begin(), relevant_list_.end(), value) !=
         relevant_list_.end();
}

} // namespace base
} // namespace hamstring

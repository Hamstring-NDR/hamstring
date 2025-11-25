#pragma once

#include "hamstring/detector/feature_extractor.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declaration for ONNX Runtime classes to avoid exposing them in the
// header
namespace onnxruntime {
class InferenceSession;
class Env;
class SessionOptions;
class RunOptions;
} // namespace onnxruntime

namespace hamstring {
namespace detector {

class Detector {
public:
  Detector();
  ~Detector();

  // Load ONNX model from file
  void load_model(const std::string &model_path);

  // Predict probability of domain being DGA (0.0 - 1.0)
  float predict(const std::string &domain);

private:
  FeatureExtractor feature_extractor_;

  // Pimpl idiom for ONNX Runtime objects
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace detector
} // namespace hamstring

#include "hamstring/detector/detector.hpp"
#include <onnxruntime_cxx_api.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vector>

namespace hamstring {
namespace detector {

struct Detector::Impl {
  Ort::Env env;
  Ort::SessionOptions session_options;
  std::unique_ptr<Ort::Session> session;
  Ort::AllocatorWithDefaultOptions allocator;

  std::vector<const char *> input_node_names;
  std::vector<const char *> output_node_names;
  std::vector<int64_t> input_node_dims;

  Impl()
      : env(ORT_LOGGING_LEVEL_WARNING, "HamstringDetector"), session_options() {
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_BASIC);
  }
};

Detector::Detector() : impl_(std::make_unique<Impl>()) {}

Detector::~Detector() = default;

void Detector::load_model(const std::string &model_path) {
  try {
    impl_->session = std::make_unique<Ort::Session>(
        impl_->env, model_path.c_str(), impl_->session_options);

    // Get input metadata
    size_t num_input_nodes = impl_->session->GetInputCount();
    impl_->input_node_names.reserve(num_input_nodes);

    for (size_t i = 0; i < num_input_nodes; i++) {
      auto input_name =
          impl_->session->GetInputNameAllocated(i, impl_->allocator);
      impl_->input_node_names.push_back(
          strdup(input_name.get())); // Need to copy because smart pointer dies

      auto type_info = impl_->session->GetInputTypeInfo(i);
      auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
      impl_->input_node_dims = tensor_info.GetShape();
    }

    // Get output metadata
    size_t num_output_nodes = impl_->session->GetOutputCount();
    impl_->output_node_names.reserve(num_output_nodes);

    for (size_t i = 0; i < num_output_nodes; i++) {
      auto output_name =
          impl_->session->GetOutputNameAllocated(i, impl_->allocator);
      impl_->output_node_names.push_back(strdup(output_name.get()));
    }

    spdlog::info("Loaded detector model from {}", model_path);

  } catch (const Ort::Exception &e) {
    spdlog::error("Failed to load ONNX model: {}", e.what());
    throw std::runtime_error("Failed to load ONNX model: " +
                             std::string(e.what()));
  }
}

float Detector::predict(const std::string &domain) {
  if (!impl_->session) {
    throw std::runtime_error("Model not loaded");
  }

  // Extract features
  auto features = feature_extractor_.extract(domain);
  std::vector<float> input_tensor_values = features.to_vector();

  // Create input tensor
  // Assuming batch size of 1
  std::vector<int64_t> input_shape = {
      1, static_cast<int64_t>(input_tensor_values.size())};

  auto memory_info =
      Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  auto input_tensor = Ort::Value::CreateTensor<float>(
      memory_info, input_tensor_values.data(), input_tensor_values.size(),
      input_shape.data(), input_shape.size());

  // Run inference
  auto output_tensors = impl_->session->Run(
      Ort::RunOptions{nullptr}, impl_->input_node_names.data(), &input_tensor,
      1, impl_->output_node_names.data(), 1);

  // Get output
  // Assuming output is a single float (probability) or [1, 1] tensor
  float *floatarr = output_tensors.front().GetTensorMutableData<float>();

  // If model outputs logits/probabilities, we might need to process.
  // Assuming the model outputs a probability score directly for now.
  // If it outputs 2 values (prob class 0, prob class 1), we'd take the second.
  // Let's assume for now it's a single value.

  return floatarr[0];
}

} // namespace detector
} // namespace hamstring

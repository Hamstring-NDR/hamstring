#!/usr/bin/env python3
"""
Convert XGBoost and RandomForest models to ONNX format for C++ inference.
"""

import os
import sys
import pickle
import argparse
from pathlib import Path

import numpy as np
import onnx
from skl2onnx import convert_sklearn
from skl2onnx.common.data_types import FloatTensorType
import onnxmltools
from onnxmltools.convert import convert_xgboost

sys.path.append(os.getcwd())
from src.base.log_config import get_logger

logger = get_logger("model_conversion")


def convert_xgboost_model(model_path: Path, output_path: Path):
    """Convert XGBoost model to ONNX format."""
    logger.info(f"Converting XGBoost model: {model_path}")
    
    # Load the model
    with open(model_path, 'rb') as f:
        model = pickle.load(f)
    
    # Define input shape (number of features)
    # Based on feature_extractor.cpp: 3 + 26 + 12 + 3 = 44 features
    initial_type = [('float_input', FloatTensorType([None, 44]))]
    
    # Convert to ONNX
    onnx_model = convert_xgboost(model, initial_types=initial_type)
    
    # Save ONNX model
    onnx.save_model(onnx_model, str(output_path))
    logger.info(f"Saved ONNX model to: {output_path}")
    
    return onnx_model


def convert_randomforest_model(model_path: Path, output_path: Path):
    """Convert RandomForest model to ONNX format."""
    logger.info(f"Converting RandomForest model: {model_path}")
    
    # Load the model
    with open(model_path, 'rb') as f:
        model = pickle.load(f)
    
    # Define input shape
    initial_type = [('float_input', FloatTensorType([None, 44]))]
    
    # Convert to ONNX
    onnx_model = convert_sklearn(model, initial_types=initial_type)
    
    # Save ONNX model
    onnx.save_model(onnx_model, str(output_path))
    logger.info(f"Saved ONNX model to: {output_path}")
    
    return onnx_model


def verify_conversion(original_model_path: Path, onnx_model_path: Path, num_samples: int = 100):
    """Verify that ONNX model produces same results as original."""
    logger.info("Verifying ONNX conversion...")
    
    # Load original model
    with open(original_model_path, 'rb') as f:
        original_model = pickle.load(f)
    
    # Load ONNX model
    import onnxruntime as rt
    sess = rt.InferenceSession(str(onnx_model_path))
    input_name = sess.get_inputs()[0].name
    
    # Generate random test data
    np.random.seed(42)
    test_data = np.random.randn(num_samples, 44).astype(np.float32)
    
    # Get predictions from original model
    original_preds = original_model.predict_proba(test_data)[:, 1]
    
    # Get predictions from ONNX model
    onnx_preds = sess.run(None, {input_name: test_data})[1][:, 1]
    
    # Compare results
    max_diff = np.max(np.abs(original_preds - onnx_preds))
    mean_diff = np.mean(np.abs(original_preds - onnx_preds))
    
    logger.info(f"Max difference: {max_diff}")
    logger.info(f"Mean difference: {mean_diff}")
    
    if max_diff < 1e-5:
        logger.info("✓ Conversion verified successfully!")
        return True
    else:
        logger.warning(f"⚠ Large difference detected: {max_diff}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Convert ML models to ONNX format")
    parser.add_argument("--model-dir", type=str, default="./models",
                       help="Directory containing trained models")
    parser.add_argument("--output-dir", type=str, default="./models/onnx",
                       help="Directory to save ONNX models")
    parser.add_argument("--verify", action="store_true",
                       help="Verify conversion accuracy")
    
    args = parser.parse_args()
    
    model_dir = Path(args.model_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Find all model files
    xgb_models = list(model_dir.glob("*xgb*.pkl"))
    rf_models = list(model_dir.glob("*rf*.pkl"))
    
    logger.info(f"Found {len(xgb_models)} XGBoost models and {len(rf_models)} RandomForest models")
    
    # Convert XGBoost models
    for model_path in xgb_models:
        output_path = output_dir / f"{model_path.stem}.onnx"
        convert_xgboost_model(model_path, output_path)
        
        if args.verify:
            verify_conversion(model_path, output_path)
    
    # Convert RandomForest models
    for model_path in rf_models:
        output_path = output_dir / f"{model_path.stem}.onnx"
        convert_randomforest_model(model_path, output_path)
        
        if args.verify:
            verify_conversion(model_path, output_path)
    
    logger.info("Model conversion complete!")


if __name__ == "__main__":
    main()

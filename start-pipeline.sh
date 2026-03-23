#!/bin/bash
# Start the complete HAMSTRING pipeline with Zeek network capture

set -e

CONFIG_FILE="${1:-config-test.yaml}"
CPP_BUILD_DIR="cpp/build/src"

echo "🚀 Starting HAMSTRING Pipeline with Zeek Integration"
echo "=================================================="
echo ""

# Check if Zeek is installed
if ! command -v zeek &> /dev/null; then
    echo "❌ Zeek is not installed. Please install Zeek first."
    exit 1
fi

# Check if config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "❌ Config file not found: $CONFIG_FILE"
    exit 1
fi

echo "📋 Configuration: $CONFIG_FILE"
echo ""

# Function to start a module in background
start_module() {
    local name=$1
    local cmd=$2
    echo "▶️  Starting $name..."
    $cmd &
    echo "   PID: $!"
}

# Start Zeek for network capture
echo "🔍 Starting Zeek Network Capture"
echo "================================"
start_module "Zeek" "python -m src.zeek.zeek_handler -c $CONFIG_FILE"
sleep 2

echo ""
echo "🔧 Starting C++ Pipeline Modules"
echo "================================"

# Start LogServer
if [ -x "$CPP_BUILD_DIR/logserver/logserver" ]; then
    start_module "LogServer" "$CPP_BUILD_DIR/logserver/logserver $CONFIG_FILE"
else
    echo "⚠️  LogServer binary not found, skipping"
fi
sleep 1

# Start LogCollector
if [ -x "$CPP_BUILD_DIR/logcollector/logcollector" ]; then
    start_module "LogCollector" "$CPP_BUILD_DIR/logcollector/logcollector $CONFIG_FILE"
else
    echo "⚠️  LogCollector binary not found, skipping"
fi
sleep 1

# Start Prefilter
if [ -x "$CPP_BUILD_DIR/prefilter/prefilter" ]; then
    start_module "Prefilter" "$CPP_BUILD_DIR/prefilter/prefilter $CONFIG_FILE"
else
    echo "⚠️  Prefilter binary not found, skipping"
fi
sleep 1

# Start Inspector (with ML anomaly detection)
if [ -x "$CPP_BUILD_DIR/inspector/inspector" ]; then
    start_module "Inspector (ML)" "$CPP_BUILD_DIR/inspector/inspector $CONFIG_FILE"
else
    echo "⚠️  Inspector binary not found, skipping"
fi

echo ""
echo "✅ All modules started!"
echo ""
echo "📊 Monitor pipeline:"
echo "   - Zeek input:     docker exec kafka1 kafka-console-consumer --bootstrap-server localhost:19092 --topic pipeline-logserver_in-dns"
echo "   - LogCollector:   docker exec kafka1 kafka-console-consumer --bootstrap-server localhost:19092 --topic pipeline-logserver_to_collector-dns"
echo "   - Inspector out:  docker exec kafka1 kafka-console-consumer --bootstrap-server localhost:19092 --topic pipeline-prefilter_to_inspector-dga_inspector"
echo ""
echo "🛑 Stop all: pkill -f 'zeek|logserver|logcollector|prefilter|inspector'"
echo ""
echo "Press Ctrl+C to stop all modules"

# Wait for interrupt
trap 'echo ""; echo "🛑 Stopping all modules..."; pkill -f "zeek|logserver|logcollector|prefilter|inspector"; exit 0' INT

# Keep script running
wait

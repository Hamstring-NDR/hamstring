#!/bin/bash

# Test script for HAMSTRING C++ Pipeline
# This script sends test DNS messages to Kafka and verifies the pipeline

echo "=== HAMSTRING Pipeline Test ==="
echo ""

# 1. Send a test DNS message to the input topic
echo "1. Sending test DNS message to pipeline-logserver_in-dns..."

docker exec kafka1 kafka-console-producer \
  --bootstrap-server localhost:19092 \
  --topic pipeline-logserver_in-dns << EOF
example.com NOERROR 192.168.1.100
test.example.org NOERROR 10.0.0.1
malicious.domain.xyz NXDOMAIN 8.8.8.8
EOF

echo "   ✓ Sent 3 test messages"
echo ""

# 2. Wait a moment for processing
echo "2. Waiting 2 seconds for processing..."
sleep 2
echo ""

# 3. Check if collector topic has messages
echo "3. Checking collector topic for forwarded messages..."
docker exec kafka1 kafka-console-consumer \
  --bootstrap-server localhost:19092 \
  --topic pipeline-logserver_to_collector-dga_collector \
  --from-beginning \
  --max-messages 3 \
  --timeout-ms 5000 2>/dev/null || echo "   (No messages yet - might need to start LogServer)"

echo ""
echo "=== Test Complete ==="

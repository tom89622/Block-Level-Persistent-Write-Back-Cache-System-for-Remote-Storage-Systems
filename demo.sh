#!/bin/bash

REPEAT=1
IFACE="eth0"
PORT=3260
DEVICE="/dev/nbd0"
CACHE_PROGRAM="./milestone1"
SERVER_DISK="/dev/sdb"  # Replace with actual disk device
OUTPUT_DIR="results"
LOG_DIR="cache_logs"
DATA_SIZE="10M"

mkdir -p $OUTPUT_DIR
mkdir -p $LOG_DIR

BANDWIDTHS=("20Mbit")
MODES=("read" "write")

# Clear old bandwidth limits
sudo tc qdisc del dev $IFACE root 2>/dev/null
sudo tc qdisc del dev $IFACE ingress 2>/dev/null

for MODE in "${MODES[@]}"; do
  for BW in "${BANDWIDTHS[@]}"; do
    for i in $(seq 1 $REPEAT); do

      ########################################################
      #                1️⃣ Baseline Test (direct to disk)
      ########################################################
      echo "===================================================="
      echo "Baseline mode=$MODE, bandwidth=$BW, test #$i (Zipf distribution)"

      sudo sync
      echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
      sudo blockdev --setra 0 $SERVER_DISK

      # Set bidirectional bandwidth limit
      sudo tc qdisc del dev $IFACE root 2>/dev/null
      sudo tc qdisc del dev $IFACE ingress 2>/dev/null
      if [ "$BW" != "1000Mbps" ]; then
        sudo tc qdisc add dev $IFACE root handle 1: htb default 11
        sudo tc class add dev $IFACE parent 1: classid 1:1 htb rate 1000Mbit
        sudo tc class add dev $IFACE parent 1:1 classid 1:11 htb rate $BW

        sudo tc qdisc add dev $IFACE handle ffff: ingress
        sudo tc filter add dev $IFACE parent ffff: protocol ip u32 match u32 0 0 police rate $BW burst 10k drop flowid :1

    
    ##    echo "📦 Current EGRESS limit:"
    ##    sudo tc -s qdisc show dev $IFACE

    ##    echo "📥 Current INGRESS limit:"
    ##    sudo tc -s filter show dev $IFACE parent ffff:
      fi

      if [ "$MODE" = "read" ]; then
        RW_MODE="randread"
      else
        RW_MODE="randwrite"
      fi

      BASELINE_RESULT_FILE=${OUTPUT_DIR}/baseline_${MODE}_zipf_${BW}_${i}.json
      echo "Running baseline fio ($RW_MODE, zipf)..."
      sudo fio --name=test_io_baseline \
          --filename=$SERVER_DISK \
          --rw=$RW_MODE \
          --bs=4k \
          --iodepth=32 \
          --numjobs=4 \
          --size=$DATA_SIZE \
          --random_distribution=zipf \
          --ioengine=io_uring \
          --direct=1 \
          --group_reporting \
          --output-format=json \
          > "$BASELINE_RESULT_FILE"

      echo "🔍 Baseline performance:"
      jq '.jobs[] | {job: .jobname, iops: .'"$MODE"'.iops, Throughput_MBps: (.'"$MODE"'.bw / 1024), runtime_ms: .'"$MODE"'.runtime}' "$BASELINE_RESULT_FILE"

      ########################################################
      #              2️⃣ Cache System Test (write to nbd)
      ########################################################
      echo "===================================================="
      echo "Cache mode=$MODE, bandwidth=$BW, test #$i (Zipf distribution)"

      echo "Clearing ./0 and flushing system page cache"
      sudo rm -r 0 2>/dev/null
      sudo sync
      echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
      sudo blockdev --setra 0 $DEVICE

      LOG_FILE=${LOG_DIR}/${MODE}_zipf_${BW}_${i}.log
      echo "Starting milestone1..."
      sudo $CACHE_PROGRAM 4096 $DEVICE $SERVER_DISK > "$LOG_FILE" &
      CACHE_PID=$!
      sleep 2

      RESULT_FILE=${OUTPUT_DIR}/${MODE}_zipf_${BW}_${i}.json
      echo "Running fio ($RW_MODE, zipf)..."
      sudo fio --name=test_io \
          --filename=$DEVICE \
          --rw=$RW_MODE \
          --bs=4k \
          --iodepth=32 \
          --numjobs=4 \
          --size=$DATA_SIZE \
          --random_distribution=zipf \
          --ioengine=io_uring \
          --direct=1 \
          --group_reporting \
          --output-format=json \
          > "$RESULT_FILE"
      
        sudo kill $CACHE_PID
      wait $CACHE_PID 2>/dev/null
       echo "🔍 Cache performance:"
      jq '.jobs[] | {job: .jobname, iops: .'"$MODE"'.iops, Throughput_MBps: (.'"$MODE"'.bw / 1024), runtime_ms: .'"$MODE"'.runtime}' "$RESULT_FILE"


    done
  done
done

# Clear all bandwidth limits
sudo tc qdisc del dev $IFACE root 2>/dev/null
sudo tc qdisc del dev $IFACE ingress 2>/dev/null
echo "✅ All Baseline & Cache tests completed. Results saved in $OUTPUT_DIR"
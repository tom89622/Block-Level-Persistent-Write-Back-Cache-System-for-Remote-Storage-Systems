#!/bin/bash

REPEAT=3
IFACE="eth0"
PORT=3260
DEVICE="/dev/nbd0"
CACHE_PROGRAM="./milestone1"
SERVER_DISK="/dev/sdb"  # 替換為實際磁碟設備
OUTPUT_DIR="results"
LOG_DIR="cache_logs"
DATA_SIZE="10G"

mkdir -p $OUTPUT_DIR
mkdir -p $LOG_DIR

BANDWIDTHS=("200Mbit" "100Mbit" "50Mbit" "20Mbit" "10Mbit")
MODES=("read" "write")

# 清除舊有帶寬限制
sudo tc qdisc del dev $IFACE root 2>/dev/null
sudo tc qdisc del dev $IFACE ingress 2>/dev/null

for MODE in "${MODES[@]}"; do
  for BW in "${BANDWIDTHS[@]}"; do
    for i in $(seq 1 $REPEAT); do

      ########################################################
      #                1️⃣ Baseline 測試 (直寫硬碟)
      ########################################################
      echo "===================================================="
      echo "Baseline 模式=$MODE, 帶寬=$BW, 第 $i 次測試 (Zipf 分布)"

      sudo sync
      echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
      sudo blockdev --setra 0 $SERVER_DISK

      # 設定雙向頻寬限制
      sudo tc qdisc del dev $IFACE root 2>/dev/null
      sudo tc qdisc del dev $IFACE ingress 2>/dev/null
      if [ "$BW" != "1000Mbps" ]; then
        sudo tc qdisc add dev $IFACE root handle 1: htb default 11
        sudo tc class add dev $IFACE parent 1: classid 1:1 htb rate 1000Mbit
        sudo tc class add dev $IFACE parent 1:1 classid 1:11 htb rate $BW

        sudo tc qdisc add dev $IFACE handle ffff: ingress
        sudo tc filter add dev $IFACE parent ffff: protocol ip u32 match u32 0 0 police rate $BW burst 10k drop flowid :1

        echo "📦 當前 EGRESS 限制："
        sudo tc -s qdisc show dev $IFACE

        echo "📥 當前 INGRESS 限制："
        sudo tc -s filter show dev $IFACE parent ffff:
      fi

      if [ "$MODE" = "read" ]; then
        RW_MODE="randread"
      else
        RW_MODE="randwrite"
      fi

      BASELINE_RESULT_FILE=${OUTPUT_DIR}/baseline_${MODE}_zipf_${BW}_${i}.log
      echo "執行 baseline fio ($RW_MODE, zipf)..."
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
          > "$BASELINE_RESULT_FILE"
      
      # Wait for the baseline test to finish before proceeding with the cache test
      wait $!

      ########################################################
      #              2️⃣ Cache 系統測試 (寫入 nbd)
      ########################################################
      echo "===================================================="
      echo "Cache 模式=$MODE, 帶寬=$BW, 第 $i 次測試 (Zipf 分布)"

      echo "清除 ./0 並刷新系統頁面快取"
      sudo rm -r 0
      sudo sync
      echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
      sudo blockdev --setra 0 $DEVICE

      LOG_FILE=${LOG_DIR}/${MODE}_zipf_${BW}_${i}.log
      echo "啟動 milestone1..."
      sudo $CACHE_PROGRAM 4096 $DEVICE $SERVER_DISK > "$LOG_FILE" &
      CACHE_PID=$!
      sleep 2

      RESULT_FILE=${OUTPUT_DIR}/${MODE}_zipf_${BW}_${i}.log
      echo "執行 fio ($RW_MODE, zipf)..."
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
          > "$RESULT_FILE"

      echo "結束 milestone1..."
      sudo kill $CACHE_PID
      wait $CACHE_PID 2>/dev/null

    done
  done
done

# 清理所有頻寬限制
sudo tc qdisc del dev $IFACE root 2>/dev/null
sudo tc qdisc del dev $IFACE ingress 2>/dev/null
echo "✅ 所有 Baseline & Cache 測試完成，結果保存在 $OUTPUT_DIR"

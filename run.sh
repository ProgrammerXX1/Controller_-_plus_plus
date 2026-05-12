#!/bin/bash
# Полная изоляция бенча:
#   CPU:    ядра 0,1,8,9 (2 физ. с HT)
#   RAM:    4 GiB hard cap, swap = 0
#   Disk:   500 MB/s read, 440 MB/s write (~10% AORUS Gen4 NVMe)
#   Pages:  huge pages если зарезервированы (см. apply_hugepages.sh)
#
#   ./run.sh                  обычный запуск
#   ./run.sh --cold           drop_caches перед запуском
#   ./run.sh --build          пересобрать qs
#   ./run.sh --build --cold   и то и другое

set -euo pipefail

BENCH=./qs
SRC=quicksort_levels.cpp
NVME=/dev/nvme0n1
READ_BPS=500M
WRITE_BPS=440M
MEM_MAX=4G
CPUS=0,1,8,9

COLD=0
BUILD=0
for arg in "$@"; do
    case "$arg" in
        --cold)  COLD=1 ;;
        --build) BUILD=1 ;;
        *) echo "unknown arg: $arg"; exit 1 ;;
    esac
done

cd "$(dirname "$0")"

if [ $BUILD -eq 1 ] || [ ! -x "$BENCH" ]; then
    echo "=== build ==="
    g++ -O3 -std=c++20 -march=native -DNDEBUG -o qs "$SRC"
fi

if [ $COLD -eq 1 ]; then
    echo "=== drop page cache ==="
    sync
    sudo bash -c 'echo 3 > /proc/sys/vm/drop_caches'
fi

echo "=== isolation ==="
echo "  CPU:    ${CPUS}"
echo "  RAM:    ${MEM_MAX} (swap=0)"
echo "  Disk:   read=${READ_BPS}/s, write=${WRITE_BPS}/s on ${NVME}"
echo

# systemd-run --scope: транзитный cgroup со всеми лимитами, удаляется после выхода.
exec sudo systemd-run --scope --quiet \
    --uid="$(id -u)" --gid="$(id -g)" \
    --setenv=HOME="$HOME" \
    -p MemoryMax=${MEM_MAX} \
    -p MemorySwapMax=0 \
    -p AllowedCPUs=${CPUS} \
    -p IOReadBandwidthMax="${NVME} ${READ_BPS}" \
    -p IOWriteBandwidthMax="${NVME} ${WRITE_BPS}" \
    "$BENCH"

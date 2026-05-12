#!/bin/bash
# Запуск бенча в изолированной среде:
#   - CPU: только ядра 0,1,8,9 (две физ. пары)
#   - RAM: hard cap 4 GB через cgroup v2 memory.max
#   - Swap: запрещён (memory.swap.max=0)
#   - Page cache: сбрасывается перед каждым прогоном (опция --cold)
#
# Использование:
#   ./run.sh                       # обычный прогон, page cache как есть
#   ./run.sh --cold                # drop_caches перед прогоном (холодный кеш)
#   ./run.sh --build               # пересобрать qs и запустить
#   ./run.sh --build --cold        # пересобрать + холодный прогон

set -euo pipefail

BENCH=./qs
SRC=quicksort_levels.cpp
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
    echo "  qs $(stat -c%s qs) bytes"
fi

if [ $COLD -eq 1 ]; then
    echo "=== drop page cache (нужен sudo) ==="
    sync
    sudo bash -c 'echo 3 > /proc/sys/vm/drop_caches'
    echo "  caches dropped"
fi

echo "=== isolation ==="
echo "  CPU mask:     0,1,8,9 (две физ. пары с HT)"
echo "  Memory cap:   4 GiB"
echo "  Swap:         disabled (memory.swap.max=0)"
echo

echo "=== launching ==="
# systemd-run --scope создаёт транзитный cgroup, запускает в нём, и удаляет после.
# --user НЕ работает с AllowedCPUs/MemoryMax (нужны root capabilities на cgroup),
# поэтому --system + sudo, но запускаем под нашим uid.
exec sudo systemd-run --scope --quiet \
    --uid="$(id -u)" --gid="$(id -g)" \
    --setenv=HOME="$HOME" \
    -p MemoryMax=4G \
    -p MemorySwapMax=0 \
    -p AllowedCPUs=0,1,8,9 \
    -p IOWeight=100 \
    "$BENCH"

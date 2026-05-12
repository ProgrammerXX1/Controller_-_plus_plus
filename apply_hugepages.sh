#!/bin/bash
# Резервирует 512 × 2 MB huge pages (= 1 GiB) при старте ядра.
# Делает backup /etc/default/grub, правит, запускает update-grub.
# Reboot НЕ делает — это решение пользователя.
#
# Запуск:
#   sudo ./apply_hugepages.sh
#
# Откат: sudo cp /etc/default/grub.bak.<TS> /etc/default/grub && sudo update-grub

set -euo pipefail

if [ "$EUID" -ne 0 ]; then
    echo "нужен root: sudo $0"
    exit 1
fi

TS=$(date +%Y%m%d-%H%M%S)
BACKUP="/etc/default/grub.bak.${TS}"

echo "=== backup /etc/default/grub -> ${BACKUP} ==="
cp -v /etc/default/grub "${BACKUP}"

CURRENT=$(grep '^GRUB_CMDLINE_LINUX=' /etc/default/grub)
echo "before: ${CURRENT}"

# Удаляем старые huge-page параметры (если уже были), добавляем свежие в конец.
NEW_LINE=$(echo "${CURRENT}" \
    | sed -E 's/\s*hugepages=[0-9]+//g; s/\s*default_hugepagesz=[0-9A-Za-z]+//g; s/\s*transparent_hugepage=[a-z]+//g' \
    | sed -E 's|"$| hugepages=512 default_hugepagesz=2M transparent_hugepage=madvise"|')

echo "after:  ${NEW_LINE}"

# Применяем
sed -i "s|${CURRENT}|${NEW_LINE}|" /etc/default/grub

echo
echo "=== update-grub ==="
update-grub 2>&1 | tail -10

echo
echo "============================================================"
echo "Параметры добавлены. Применятся ПОСЛЕ перезагрузки:"
echo "  hugepages=512 default_hugepagesz=2M transparent_hugepage=madvise"
echo
echo "После reboot проверь:"
echo "  grep -E 'HugePages_Total|Hugepagesize' /proc/meminfo"
echo "  ожидание: HugePages_Total = 512, Hugepagesize = 2048 kB"
echo
echo "Откат (если что):"
echo "  sudo cp ${BACKUP} /etc/default/grub && sudo update-grub && sudo reboot"
echo "============================================================"

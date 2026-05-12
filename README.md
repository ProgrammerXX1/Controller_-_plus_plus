# tests

Стенд для замеров производительности C++ с жёсткой изоляцией CPU, RAM и I/O.

## Структура

```
tests/
├── README.md
├── quicksort_levels.cpp     # пять реализаций quicksort на разных уровнях абстракции
├── run.sh                   # запуск с CPU pin + RAM cap 4G + swap off
├── apply_hugepages.sh       # одноразовый: добавляет hugepages=512 в GRUB (нужен sudo + reboot)
└── data/
    └── hn_1m.jsonl          # 1 000 000 записей Hacker News, 310 MB (gitignore)
```

## Аппаратное окружение

- **CPU:** Intel i9-11900, 8 физ. ядер × 2 HT = 16 логических, single NUMA, 16 MiB L3
- **RAM:** 31 GiB DDR4
- **Disk:** GIGABYTE AORUS GP-ASM2NE6200TTTD, NVMe Gen4 (~5 GB/s read, ~4.4 GB/s write), ext4 на `/`
- **OS:** Kali Linux, kernel 6.8.0-111-generic

## Изоляция — три слоя

### Слой 1. CPU (через GRUB, уже применено)
```
isolcpus=0,1,8,9  nohz_full=0,1,8,9  rcu_nocbs=0,1,8,9
```
Изолированы две полные физические пары: CPU0↔CPU8 и CPU1↔CPU9. На «общих» 12 логических ядрах (2-7, 10-15) живёт вся остальная система. Ни один user‑процесс не попадёт на 0,1,8,9 без явного `taskset` или `AllowedCPUs`.

### Слой 2. RAM (через cgroup v2, runtime — в `run.sh`)
Для бенчей задаётся транзитный systemd scope с:
- `MemoryMax=4G` — hard cap на RSS; превышение → OOM kill
- `MemorySwapMax=0` — swap запрещён
- `AllowedCPUs=0,1,8,9` — дополнительная фиксация через cgroup
- `IOWeight=100` — приоритет на диске

Внутри программы дополнительно:
- `mlockall(MCL_CURRENT | MCL_FUTURE)` — страницы не свопаются
- `madvise(buf, len, MADV_HUGEPAGE)` — горячие буферы на 2 MB страницах

### Слой 3. Huge pages (опционально, через GRUB, требует reboot)
После `sudo ./apply_hugepages.sh && sudo reboot`:
```
hugepages=512 default_hugepagesz=2M transparent_hugepage=madvise
```
Это резервирует **1 GiB в виде 512 × 2 MB страниц** при старте ядра. Программа получает их через `mmap(..., MAP_HUGETLB)` или `madvise(MADV_HUGEPAGE)`. Эффект: меньше TLB‑промахов, нет page faults на горячем пути.

## Запуск

```bash
./run.sh                  # обычный прогон (page cache как есть)
./run.sh --cold           # drop_caches перед прогоном (холодное чтение с NVMe)
./run.sh --build          # пересобрать qs и запустить
./run.sh --build --cold   # пересборка + холодный кеш
```

`run.sh` использует `sudo systemd-run` — потребуется ввод пароля.

## quicksort_levels.cpp

Пять реализаций сортировки 5M случайных `int`, best-of-5 каждой, проверка `std::is_sorted` после каждого прогона.

| # | Уровень | Что демонстрирует |
|---|---|---|
| 1 | `std::sort` | эталон: introsort + insertion sort на коротких диапазонах |
| 2 | Идиоматичный | шаблоны + итераторы + value semantics |
| 3 | Сырые указатели | C-style, Lomuto partition |
| 4 | Итеративный | свой стек на арене, без рекурсии (без риска stack overflow) |
| 5 | Оптимизированный | median-of-three + Hoare partition + insertion sort на малых + tail-call elim |

Типичный результат на изолированных ядрах с `-O3 -march=native`:

```
1. std::sort (STL)               ~300 ms
2. Idiomatic (templates)         ~365 ms
3. Raw pointers (C-style)        ~345 ms
4. Arena stack (iterative)       ~330 ms
5. Optimized (all tricks)        ~315 ms
```

`std::sort` обычно побеждает или идёт ноздря в ноздрю с уровнем 5 — потому что в нём те же трюки плюс годы fine-tuning под конкретные архитектуры.

## data/hn_1m.jsonl

1 000 000 записей Hacker News, JSON per line:
```json
{"id": "...", "title": "...", "author": "...", "url": "...",
 "text": "...", "points": N, "num_comments": N, "created_at_i": <unix_ts>}
```

| Параметр | Значение |
|---|---|
| Размер | 310 MB |
| Записей | ровно 1 000 000 |
| Покрытие | ~18 лет (2008-03 — 2026-05) |
| Длина `text` | median = 0, mean = 136 B, p95 = 1 KB |

Файл в `.gitignore` (>100 MB лимит GitHub). Используется как канонический workload для бенчмарков индексации, JSON-парсинга, hash-table нагрузок и I/O экспериментов.

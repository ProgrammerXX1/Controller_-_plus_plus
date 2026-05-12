// Quicksort на 5 уровнях абстракции
// Компиляция: g++ -O3 -std=c++20 -o qs quicksort_levels.cpp

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using Clock = std::chrono::steady_clock;

// ============================================================
// УРОВЕНЬ 1: STL std::sort — production-стандарт
// ============================================================
// Реально использует introsort: quicksort → heapsort при глубине O(log N),
// + insertion sort на маленьких диапазонах. Это эталон.
void level1_std_sort(std::vector<int>& v) {
    std::sort(v.begin(), v.end());
}

// ============================================================
// УРОВЕНЬ 2: Идиоматичный C++ — шаблоны + итераторы
// ============================================================
// Работает с любым RandomAccessIterator. Value semantics.
template <typename It>
void quicksort_idiomatic(It first, It last) {
    auto n = last - first;
    if (n < 2) return;

    auto pivot = *(first + n / 2);
    auto mid1 = std::partition(first, last, [pivot](const auto& x) { return x < pivot; });
    auto mid2 = std::partition(mid1, last, [pivot](const auto& x) { return !(pivot < x); });

    quicksort_idiomatic(first, mid1);
    quicksort_idiomatic(mid2, last);
}

void level2_idiomatic(std::vector<int>& v) {
    quicksort_idiomatic(v.begin(), v.end());
}

// ============================================================
// УРОВЕНЬ 3: Сырые указатели — C-style
// ============================================================
// Никаких итераторов, никаких шаблонов. Указатель + длина.
// Indexing через арифметику указателей. Lomuto partition.
static void quicksort_raw(int* arr, int lo, int hi) {
    if (lo >= hi) return;

    int pivot = arr[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; ++j) {
        if (arr[j] < pivot) {
            ++i;
            int t = arr[i]; arr[i] = arr[j]; arr[j] = t;  // swap вручную
        }
    }
    int t = arr[i + 1]; arr[i + 1] = arr[hi]; arr[hi] = t;
    int p = i + 1;

    quicksort_raw(arr, lo, p - 1);
    quicksort_raw(arr, p + 1, hi);
}

void level3_raw_pointers(std::vector<int>& v) {
    quicksort_raw(v.data(), 0, (int)v.size() - 1);
}

// ============================================================
// УРОВЕНЬ 4: Итеративный + ручной стек на арене
// ============================================================
// Никакой рекурсии — собственный стек диапазонов в заранее выделенном буфере.
// Это "арена-стиль": одна аллокация, никакого нового malloc в горячем цикле,
// нет риска stack overflow на больших массивах.
struct Range { int lo, hi; };

void level4_arena_stack(std::vector<int>& v) {
    if (v.size() < 2) return;
    int* arr = v.data();

    // Глубина quicksort в худшем случае O(N), но в среднем O(log N).
    // Резервируем верхнюю границу один раз.
    constexpr size_t MAX_DEPTH = 64;  // хватит для N до 2^64
    Range stack[MAX_DEPTH];
    int sp = 0;
    stack[sp++] = {0, (int)v.size() - 1};

    while (sp > 0) {
        Range r = stack[--sp];
        if (r.lo >= r.hi) continue;

        int pivot = arr[r.hi];
        int i = r.lo - 1;
        for (int j = r.lo; j < r.hi; ++j) {
            if (arr[j] < pivot) {
                ++i;
                int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
            }
        }
        int t = arr[i + 1]; arr[i + 1] = arr[r.hi]; arr[r.hi] = t;
        int p = i + 1;

        // PUSH больший диапазон первым — гарантирует глубину стека O(log N)
        if (p - 1 - r.lo > r.hi - (p + 1)) {
            if (r.lo < p - 1)    stack[sp++] = {r.lo, p - 1};
            if (p + 1 < r.hi)    stack[sp++] = {p + 1, r.hi};
        } else {
            if (p + 1 < r.hi)    stack[sp++] = {p + 1, r.hi};
            if (r.lo < p - 1)    stack[sp++] = {r.lo, p - 1};
        }
    }
}

// ============================================================
// УРОВЕНЬ 5: Оптимизированный — все известные трюки
// ============================================================
// 1. Insertion sort для маленьких диапазонов (cache-friendly + меньше веток)
// 2. Median-of-three для pivot (защита от худшего случая)
// 3. Hoare partition (меньше swap'ов, чем Lomuto)
// 4. Tail-call elimination через цикл (как std::sort)
static inline void insertion_sort(int* a, int lo, int hi) {
    for (int i = lo + 1; i <= hi; ++i) {
        int x = a[i];
        int j = i - 1;
        while (j >= lo && a[j] > x) {
            a[j + 1] = a[j];
            --j;
        }
        a[j + 1] = x;
    }
}

static inline int median_of_three(int* a, int lo, int hi) {
    int mid = lo + (hi - lo) / 2;
    if (a[lo] > a[mid]) std::swap(a[lo], a[mid]);
    if (a[lo] > a[hi])  std::swap(a[lo], a[hi]);
    if (a[mid] > a[hi]) std::swap(a[mid], a[hi]);
    return a[mid];
}

static void quicksort_optimized(int* a, int lo, int hi) {
    while (hi - lo > 16) {  // порог: ниже — insertion sort
        int pivot = median_of_three(a, lo, hi);

        // Hoare partition
        int i = lo - 1, j = hi + 1;
        while (true) {
            do { ++i; } while (a[i] < pivot);
            do { --j; } while (a[j] > pivot);
            if (i >= j) break;
            std::swap(a[i], a[j]);
        }

        // Рекурсия в МЕНЬШУЮ половину, итерация в большую (O(log N) стек)
        if (j - lo < hi - j) {
            quicksort_optimized(a, lo, j);
            lo = j + 1;
        } else {
            quicksort_optimized(a, j + 1, hi);
            hi = j;
        }
    }
    insertion_sort(a, lo, hi);
}

void level5_optimized(std::vector<int>& v) {
    if (v.size() < 2) return;
    quicksort_optimized(v.data(), 0, (int)v.size() - 1);
}

// ============================================================
// БЕНЧМАРК
// ============================================================
template <typename F>
double bench(const char* name, F f, const std::vector<int>& src, int runs = 5) {
    double best_ms = 1e18;
    for (int r = 0; r < runs; ++r) {
        auto v = src;  // свежая копия для каждого прогона
        auto t1 = Clock::now();
        f(v);
        auto t2 = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
        if (ms < best_ms) best_ms = ms;

        // проверка корректности
        if (!std::is_sorted(v.begin(), v.end())) {
            std::printf("  [%s] FAILED — not sorted!\n", name);
            return -1;
        }
    }
    std::printf("  %-30s %8.2f ms\n", name, best_ms);
    return best_ms;
}

int main() {
    const size_t N = 5'000'000;
    std::printf("Sorting %zu random ints, best of 5 runs\n", N);
    std::printf("Compiled: %s %s, -O%d\n\n",
#ifdef __clang__
        "clang", __clang_version__,
#elif defined(__GNUC__)
        "g++", __VERSION__,
#else
        "??", "??",
#endif
#ifdef __OPTIMIZE__
        3
#else
        0
#endif
    );

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 1'000'000'000);
    std::vector<int> src(N);
    for (auto& x : src) x = dist(rng);

    bench("1. std::sort (STL)",      level1_std_sort,      src);
    bench("2. Idiomatic (templates)", level2_idiomatic,    src);
    bench("3. Raw pointers (C-style)", level3_raw_pointers, src);
    bench("4. Arena stack (iterative)", level4_arena_stack, src);
    bench("5. Optimized (all tricks)", level5_optimized,    src);

    return 0;
}

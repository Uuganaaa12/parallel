#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <string>
#include <omp.h>

using namespace std;
using namespace std::chrono;

// 1. Атомик заагч бүхий зангилааны бүтэц
struct Node
{
    int data;
    std::atomic<Node *> next;

    Node(int v) : data(v), next(nullptr) {}
};

// ТҮГЖЭЭГҮЙ ОРУУЛАХ ФУНКЦ (LOCK-FREE INSERTION) - АЛГОРИТМЫН ЗҮРХ
void lockFreeInsert(std::atomic<Node *> &sharedHead, Node *newNode)
{
    while (true)
    {
        Node *pred = nullptr;
        Node *curr = sharedHead.load(); // Хайлтыг эхнээс нь эхлэх

        // Өөрийн байрлалыг хайх
        while (curr != nullptr && curr->data < newNode->data)
        {
            pred = curr;
            curr = curr->next.load();
        }

        // Оруулах зангилаагаа бэлтгэх (Одоогоор нийтийн жагсаалтад ороогүй)
        newNode->next.store(curr);

        // Атомик CAS үйлдэл
        if (pred == nullptr)
        {
            // Хамгийн эхэнд оруулах (Head update)
            if (sharedHead.compare_exchange_strong(curr, newNode))
            {
                break; // Амжилттай орсон тул зогсоно
            }
        }
        else
        {
            // Хоёр зангилааны голд оруулах
            if (pred->next.compare_exchange_strong(curr, newNode))
            {
                break; // Амжилттай орсон тул зогсоно
            }
        }
        // Хэрэв CAS худал буцаавал өөр Thread өрсчихсөн гэсэн үг.
        // while(true) давталт дахин ажиллаж, хайлтыг шинээр эхлүүлнэ (Retry).
    }
}

// 1. std::thread АШИГЛАСАН ПАРАЛЛЕЛ ХУВИЛБАР
Node *threadInsertionSort(Node *head, int numThreads)
{
    std::atomic<Node *> sortedHead{nullptr};
    std::vector<Node *> nodes;

    // Эрэмбэлэгдээгүй жагсаалтыг салгаж вектор руу хийх
    while (head)
    {
        Node *next = head->next.load();
        head->next.store(nullptr);
        nodes.push_back(head);
        head = next;
    }

    int n = nodes.size();
    std::vector<std::thread> workers;

    for (int t = 0; t < numThreads; ++t)
    {
        workers.emplace_back([&, t]()
                             {
            int start = (n * t) / numThreads;
            int end = (n * (t + 1)) / numThreads;
            
            // Thread бүр өөрт оногдсон зангилаануудыг дундын жагсаалт руу оруулна
            for (int i = start; i < end; ++i) {
                lockFreeInsert(sortedHead, nodes[i]);
            } });
    }

    for (auto &w : workers)
    {
        w.join();
    }

    return sortedHead.load();
}

// 2. OpenMP АШИГЛАСАН ПАРАЛЛЕЛ ХУВИЛБАР
Node *ompInsertionSort(Node *head, int numThreads)
{
    std::atomic<Node *> sortedHead{nullptr};
    std::vector<Node *> nodes;

    while (head)
    {
        Node *next = head->next.load();
        head->next.store(nullptr);
        nodes.push_back(head);
        head = next;
    }

    int n = nodes.size();

// OpenMP директив ашиглан параллелчлах (Dynamic schedule нь ачааллыг тэнцүүлнэ)
#pragma omp parallel for num_threads(numThreads) schedule(dynamic)
    for (int i = 0; i < n; ++i)
    {
        lockFreeInsert(sortedHead, nodes[i]);
    }

    return sortedHead.load();
}

// ТУСЛАХ ФУНКЦУУД (Санамсаргүй үүсгэх, Шалгах, Устгах)
Node *generateList(int n)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(1, 10000);
    Node *head = nullptr;
    for (int i = 0; i < n; ++i)
    {
        Node *newNode = new Node(dist(rng));
        newNode->next.store(head);
        head = newNode;
    }
    return head;
}

bool isSorted(Node *head)
{
    while (head && head->next.load())
    {
        if (head->data > head->next.load()->data)
            return false;
        head = head->next.load();
    }
    return true;
}

void freeList(Node *head)
{
    while (head)
    {
        Node *next = head->next.load();
        delete head;
        head = next;
    }
}

// ҮНДСЭН ПРОГРАМ (MAIN)
int main(int argc, char **argv)
{
    int N = 1000000; // 1M элемент (Use Case)
    int THREADS = 4; // 4 цөм ашиглах
    int REPEATS = 5; // Дундаж тооцоолох давталт

    if (argc > 1)
        N = stoi(argv[1]);
    if (argc > 2)
        THREADS = stoi(argv[2]);
    if (argc > 3)
        REPEATS = stoi(argv[3]);

    cout << "=== Lock-Free Concurrent Insertion Sort ===" << endl;
    cout << "N: " << N << " | Threads: " << THREADS << " | Repeats: " << REPEATS << "\n\n";

    // 1. std::thread тест (дундаж хугацаа)
    double total_build_ms_thread = 0.0;
    double total_sort_ms_thread = 0.0;
    double total_check_ms_thread = 0.0;
    bool ok_thread = true;
    for (int r = 0; r < REPEATS; ++r)
    {
        auto t1_build_start = high_resolution_clock::now();
        Node *list1 = generateList(N);
        auto t1_build_end = high_resolution_clock::now();

        auto t1_sort_start = high_resolution_clock::now();
        Node *sorted1 = threadInsertionSort(list1, THREADS);
        auto t1_sort_end = high_resolution_clock::now();

        auto t1_check_start = high_resolution_clock::now();
        ok_thread = ok_thread && isSorted(sorted1);
        auto t1_check_end = high_resolution_clock::now();

        total_build_ms_thread += duration<double, std::milli>(t1_build_end - t1_build_start).count();
        total_sort_ms_thread += duration<double, std::milli>(t1_sort_end - t1_sort_start).count();
        total_check_ms_thread += duration<double, std::milli>(t1_check_end - t1_check_start).count();
        freeList(sorted1);
    }
    double build_ms_thread = total_build_ms_thread / REPEATS;
    double sort_ms_thread = total_sort_ms_thread / REPEATS;
    double check_ms_thread = total_check_ms_thread / REPEATS;
    double exec_ms_thread = build_ms_thread + sort_ms_thread + check_ms_thread;
    double transfer_ms_thread = 0.0;
    double bytes_transfer_thread = 0.0;
    double throughput_thread = exec_ms_thread > 0.0 ? (static_cast<double>(N) / (exec_ms_thread / 1000.0)) : 0.0;
    cout << "[std::thread] Avg: " << exec_ms_thread << " ms | Sorted: "
         << (ok_thread ? "YES" : "NO") << endl;
    cout << "RESULT,std_thread," << N << "," << THREADS << "," << REPEATS << "," << exec_ms_thread
         << "," << sort_ms_thread << "," << transfer_ms_thread << "," << bytes_transfer_thread
         << "," << throughput_thread << "," << build_ms_thread << "," << check_ms_thread << endl;

    // 2. OpenMP тест (дундаж хугацаа)
    double total_build_ms_omp = 0.0;
    double total_sort_ms_omp = 0.0;
    double total_check_ms_omp = 0.0;
    bool ok_omp = true;
    for (int r = 0; r < REPEATS; ++r)
    {
        auto t2_build_start = high_resolution_clock::now();
        Node *list2 = generateList(N);
        auto t2_build_end = high_resolution_clock::now();

        auto t2_sort_start = high_resolution_clock::now();
        Node *sorted2 = ompInsertionSort(list2, THREADS);
        auto t2_sort_end = high_resolution_clock::now();

        auto t2_check_start = high_resolution_clock::now();
        ok_omp = ok_omp && isSorted(sorted2);
        auto t2_check_end = high_resolution_clock::now();

        total_build_ms_omp += duration<double, std::milli>(t2_build_end - t2_build_start).count();
        total_sort_ms_omp += duration<double, std::milli>(t2_sort_end - t2_sort_start).count();
        total_check_ms_omp += duration<double, std::milli>(t2_check_end - t2_check_start).count();
        freeList(sorted2);
    }
    double build_ms_omp = total_build_ms_omp / REPEATS;
    double sort_ms_omp = total_sort_ms_omp / REPEATS;
    double check_ms_omp = total_check_ms_omp / REPEATS;
    double exec_ms_omp = build_ms_omp + sort_ms_omp + check_ms_omp;
    double transfer_ms_omp = 0.0;
    double bytes_transfer_omp = 0.0;
    double throughput_omp = exec_ms_omp > 0.0 ? (static_cast<double>(N) / (exec_ms_omp / 1000.0)) : 0.0;
    cout << "[OpenMP     ] Avg: " << exec_ms_omp << " ms | Sorted: "
         << (ok_omp ? "YES" : "NO") << endl;
    cout << "RESULT,openmp," << N << "," << THREADS << "," << REPEATS << "," << exec_ms_omp
         << "," << sort_ms_omp << "," << transfer_ms_omp << "," << bytes_transfer_omp
         << "," << throughput_omp << "," << build_ms_omp << "," << check_ms_omp << endl;

    return 0;
}
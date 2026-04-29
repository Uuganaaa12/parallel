#include <iostream>
#include <chrono>
#include <random>
#include <string>

using namespace std;
using namespace std::chrono;

// 1. Зангилааны бүтэц
struct Node
{
    int data;
    Node *next;

    Node(int v) : data(v), next(nullptr) {}
};

// ДАРААЛСАН ОРУУЛАХ ЭРЭМБЭЛЭЛТ
Node *sequentialInsertionSort(Node *head)
{
    Node *sorted = nullptr;

    while (head)
    {
        Node *curr = head;
        head = head->next;

        if (sorted == nullptr || curr->data < sorted->data)
        {
            curr->next = sorted;
            sorted = curr;
            continue;
        }

        Node *pred = sorted;
        while (pred->next && pred->next->data < curr->data)
        {
            pred = pred->next;
        }
        curr->next = pred->next;
        pred->next = curr;
    }

    return sorted;
}

// ТУСЛАХ ФУНКЦУУД (Санамсаргүй үүсгэх, Шалгах, Устгах)
Node *generateList(int n)
{
    mt19937 rng(42);
    uniform_int_distribution<int> dist(1, 10000);
    Node *head = nullptr;
    for (int i = 0; i < n; ++i)
    {
        Node *newNode = new Node(dist(rng));
        newNode->next = head;
        head = newNode;
    }
    return head;
}

bool isSorted(Node *head)
{
    while (head && head->next)
    {
        if (head->data > head->next->data)
            return false;
        head = head->next;
    }
    return true;
}

void freeList(Node *head)
{
    while (head)
    {
        Node *next = head->next;
        delete head;
        head = next;
    }
}

void runForN(int N, int REPEATS)
{
    cout << "=== Sequential Insertion Sort ===" << endl;
    cout << "N: " << N << " | Repeats: " << REPEATS << "\n\n";

    double total_build_ms = 0.0;
    double total_sort_ms = 0.0;
    double total_check_ms = 0.0;
    bool ok = true;
    for (int r = 0; r < REPEATS; ++r)
    {
        auto t_build_start = high_resolution_clock::now();
        Node *list = generateList(N);
        auto t_build_end = high_resolution_clock::now();

        auto t_sort_start = high_resolution_clock::now();
        Node *sorted = sequentialInsertionSort(list);
        auto t_sort_end = high_resolution_clock::now();

        auto t_check_start = high_resolution_clock::now();
        ok = ok && isSorted(sorted);
        auto t_check_end = high_resolution_clock::now();

        total_build_ms += duration<double, std::milli>(t_build_end - t_build_start).count();
        total_sort_ms += duration<double, std::milli>(t_sort_end - t_sort_start).count();
        total_check_ms += duration<double, std::milli>(t_check_end - t_check_start).count();
        freeList(sorted);
    }

    double build_ms = total_build_ms / REPEATS;
    double sort_ms = total_sort_ms / REPEATS;
    double check_ms = total_check_ms / REPEATS;
    double exec_ms = build_ms + sort_ms + check_ms;
    double transfer_ms = 0.0;
    double bytes_transfer = 0.0;
    double throughput = exec_ms > 0.0 ? (static_cast<double>(N) / (exec_ms / 1000.0)) : 0.0;
    cout << "[Sequential] Avg: " << exec_ms << " ms | Sorted: "
         << (ok ? "YES" : "NO") << endl;
    cout << "RESULT,sequential," << N << "," << 1 << "," << REPEATS << "," << exec_ms
         << "," << sort_ms << "," << transfer_ms << "," << bytes_transfer << "," << throughput
         << "," << build_ms << "," << check_ms << endl;

    cout << "\n";
}

// ҮНДСЭН ПРОГРАМ (MAIN)
int main(int argc, char **argv)
{
    int REPEATS = 3; // Дундаж тооцоолох давталт

    if (argc > 2)
        REPEATS = stoi(argv[2]);

    if (argc > 1)
    {
        int N = stoi(argv[1]);
        runForN(N, REPEATS);
        return 0;
    }

    int sizes[] = {10000, 100000, 1000000};
    for (int n : sizes)
    {
        runForN(n, REPEATS);
    }

    return 0;
}

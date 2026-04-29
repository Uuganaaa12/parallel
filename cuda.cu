#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include <cuda_runtime.h>

using namespace std;
using namespace std::chrono;

// 1. Зангилааны бүтэц
struct Node
{
    int data;
    Node *next;
};


// CUDA DEVICE ФУНКЦ: ТҮГЖЭЭГҮЙ ОРУУЛАХ ФУНКЦ (LOCK-FREE INSERTION)

__device__ void lockFreeInsertDevice(Node **sharedHead, Node *newNode)
{
    while (true)
    {
        Node *pred = nullptr;
        Node *curr = *sharedHead;

        // 1. Байрлалаа хайх
        while (curr != nullptr && curr->data < newNode->data)
        {
            pred = curr;
            curr = curr->next;
        }

        // 2. Шинэ зангилаагаа дараагийнх руу холбох
        newNode->next = curr;

        // 3. Атомик CAS (Compare-And-Swap)
        unsigned long long int assumed = (unsigned long long int)curr;
        unsigned long long int old;

        if (pred == nullptr)
        {
            unsigned long long int *addr = (unsigned long long int *)sharedHead;
            old = atomicCAS(addr, assumed, (unsigned long long int)newNode);
            if (old == assumed)
                break;
        }
        else
        {
            unsigned long long int *addr = (unsigned long long int *)&(pred->next);
            old = atomicCAS(addr, assumed, (unsigned long long int)newNode);
            if (old == assumed)
                break;
        }
    }
}


// CUDA KERNEL: Thread бүр өөрийн зангилааг дундын жагсаалт руу хийнэ

__global__ void insertNodesKernel(Node **sharedHead, Node *nodes, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n)
    {
        lockFreeInsertDevice(sharedHead, &nodes[idx]);
    }
}


// ТУСЛАХ ФУНКЦУУД (Шалгах)

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


// ҮНДСЭН ПРОГРАМ (MAIN)

int main(int argc, char **argv)
{
    int N = 10000;     // 10K элемент (Use Case)
    int THREADS = 256; // threads per block
    int REPEATS = 5;   // Дундаж тооцоолох давталт

    if (argc > 1)
        N = stoi(argv[1]);
    if (argc > 2)
        THREADS = stoi(argv[2]);
    if (argc > 3)
        REPEATS = stoi(argv[3]);

    cout << "=== CUDA Lock-Free Concurrent Insertion Sort ===" << endl;
    cout << "N: " << N << " | Threads: " << THREADS << " | Repeats: " << REPEATS << "\n\n";

    // 1. Unified Memory ашиглан санах ой нөөцлөх
    Node *nodes;
    cudaMallocManaged(&nodes, N * sizeof(Node));

    Node **sharedHead;
    cudaMallocManaged(&sharedHead, sizeof(Node *));

    // 2. CUDA Kernel ажиллуулах тохиргоо
    int threadsPerBlock = THREADS;
    int blocks = (N + threadsPerBlock - 1) / threadsPerBlock;

    mt19937 rng(42);
    uniform_int_distribution<int> dist(1, 10000);

    double total_build_ms = 0.0;
    double total_kernel_ms = 0.0;
    double total_check_ms = 0.0;
    bool ok = true;

    for (int r = 0; r < REPEATS; ++r)
    {
        *sharedHead = nullptr;

        auto t_build_start = high_resolution_clock::now();
        for (int i = 0; i < N; ++i)
        {
            nodes[i].data = dist(rng);
            nodes[i].next = nullptr;
        }
        auto t_build_end = high_resolution_clock::now();

        auto t_start = high_resolution_clock::now();
        insertNodesKernel<<<blocks, threadsPerBlock>>>(sharedHead, nodes, N);
        cudaDeviceSynchronize();
        auto t_end = high_resolution_clock::now();

        auto t_check_start = high_resolution_clock::now();
        Node *finalSortedList = *sharedHead;
        ok = ok && isSorted(finalSortedList);
        auto t_check_end = high_resolution_clock::now();

        total_build_ms += duration<double, std::milli>(t_build_end - t_build_start).count();
        total_kernel_ms += duration<double, std::milli>(t_end - t_start).count();
        total_check_ms += duration<double, std::milli>(t_check_end - t_check_start).count();
    }

    double build_ms = total_build_ms / REPEATS;
    double sort_ms = total_kernel_ms / REPEATS;
    double check_ms = total_check_ms / REPEATS;
    double exec_ms = build_ms + sort_ms + check_ms;
    double transfer_ms = 0.0;
    double bytes_transfer = 0.0;
    double throughput = exec_ms > 0.0 ? (static_cast<double>(N) / (exec_ms / 1000.0)) : 0.0;

    cout << "[CUDA Lock-Free] Avg: " << exec_ms << " ms | Sorted: "
         << (ok ? "YES" : "NO") << endl;
    cout << "RESULT,cuda," << N << "," << THREADS << "," << REPEATS << "," << exec_ms
         << "," << sort_ms << "," << transfer_ms << "," << bytes_transfer << "," << throughput
         << "," << build_ms << "," << check_ms << endl;

    // 3. Санах ой чөлөөлөх
    cudaFree(nodes);
    cudaFree(sharedHead);

    return 0;
}

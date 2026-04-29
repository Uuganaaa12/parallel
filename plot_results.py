import csv
import math
from collections import defaultdict

import matplotlib.pyplot as plt


def load_results(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            row["n"] = int(row["n"])
            row["threads"] = int(row["threads"])
            row["repeats"] = int(row["repeats"])
            row["exec_ms"] = float(row["exec_ms"])
            row["sort_ms"] = float(row["sort_ms"])
            row["transfer_ms"] = float(row["transfer_ms"])
            row["bytes_transfer"] = float(row["bytes_transfer"])
            row["throughput"] = float(row["throughput"])
            row["build_ms"] = float(row.get("build_ms", 0.0))
            row["check_ms"] = float(row.get("check_ms", 0.0))
            rows.append(row)
    return rows


def group_by_method(rows):
    by_method = defaultdict(list)
    for r in rows:
        by_method[r["method"]].append(r)
    for method in by_method:
        by_method[method].sort(key=lambda x: x["n"])
    return by_method


def plot_time(by_method, out_path):
    plt.figure(figsize=(7, 4.5))
    for method, items in by_method.items():
        ns = [r["n"] for r in items]
        times = [r["exec_ms"] for r in items]
        plt.plot(ns, times, marker="o", label=method)

    plt.xscale("log")
    plt.yscale("log")
    plt.xlabel("N (elements)")
    plt.ylabel("Average time (ms)")
    plt.title("Execution Time vs N")
    plt.grid(True, which="both", linestyle="--", linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=200)


def plot_speedup(by_method, out_path):
    if "sequential" not in by_method:
        raise ValueError("Missing sequential results for speedup plot")

    seq_map = {r["n"]: r["exec_ms"] for r in by_method["sequential"]}

    plt.figure(figsize=(7, 4.5))
    for method, items in by_method.items():
        ns = [r["n"] for r in items]
        speedups = []
        for r in items:
            base = seq_map.get(r["n"])
            if base is None or base <= 0:
                speedups.append(float("nan"))
            else:
                speedups.append(base / r["exec_ms"])
        plt.plot(ns, speedups, marker="o", label=method)

    plt.xscale("log")
    plt.xlabel("N (elements)")
    plt.ylabel("Speedup (x)")
    plt.title("Speedup vs N")
    plt.grid(True, which="both", linestyle="--", linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=200)


def plot_throughput(by_method, out_path):
    plt.figure(figsize=(7, 4.5))
    for method, items in by_method.items():
        ns = [r["n"] for r in items]
        thr = [r["throughput"] for r in items]
        plt.plot(ns, thr, marker="o", label=method)

    plt.xscale("log")
    plt.yscale("log")
    plt.xlabel("N (elements)")
    plt.ylabel("Throughput (elements/sec)")
    plt.title("Throughput vs N")
    plt.grid(True, which="both", linestyle="--", linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=200)


def plot_phase_breakdown(rows, method, out_path):
    items = [r for r in rows if r["method"] == method]
    if not items:
        return

    items.sort(key=lambda x: x["n"])
    labels = [str(r["n"]) for r in items]
    build = [r["build_ms"] for r in items]
    sort = [r["sort_ms"] for r in items]
    check = [r["check_ms"] for r in items]

    x = range(len(items))
    plt.figure(figsize=(7, 4.5))
    plt.bar(x, build, label="build")
    plt.bar(x, sort, bottom=build, label="sort")
    bottom2 = [build[i] + sort[i] for i in range(len(items))]
    plt.bar(x, check, bottom=bottom2, label="check")

    plt.xticks(x, labels)
    plt.xlabel("N (elements)")
    plt.ylabel("Time (ms)")
    plt.title(f"Phase Breakdown: {method}")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=200)


def plot_cuda_transfer_vs_compute(rows, out_path):
    items = [r for r in rows if r["method"] == "cuda"]
    if not items:
        return

    items.sort(key=lambda x: x["n"])
    labels = [str(r["n"]) for r in items]
    transfer = [r["transfer_ms"] + r["build_ms"] + r["check_ms"] for r in items]
    compute = [r["sort_ms"] for r in items]

    x = range(len(items))
    plt.figure(figsize=(7, 4.5))
    plt.bar(x, transfer, label="transfer+host")
    plt.bar(x, compute, bottom=transfer, label="compute")

    plt.xticks(x, labels)
    plt.xlabel("N (elements)")
    plt.ylabel("Time (ms)")
    plt.title("CUDA: Transfer vs Compute")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=200)


def main():
    rows = load_results("results.csv")
    by_method = group_by_method(rows)

    plot_time(by_method, "diagramms/performance_time_ms.png")
    plot_speedup(by_method, "diagramms/speedup_vs_sequential.png")
    plot_throughput(by_method, "diagramms/throughput.png")

    plot_phase_breakdown(rows, "std_thread", "diagramms/phase_breakdown_multithread.png")
    plot_phase_breakdown(rows, "openmp", "diagramms/phase_breakdown_openmp.png")
    plot_phase_breakdown(rows, "cuda", "diagramms/phase_breakdown_cuda.png")
    plot_cuda_transfer_vs_compute(rows, "diagramms/cuda_transfer_vs_compute.png")

    print("Saved graphs under diagramms/")


if __name__ == "__main__":
    main()

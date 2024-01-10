import os
import subprocess
from time import perf_counter_ns
import numpy as np
import matplotlib.pyplot as plot

CWD = os.getcwd()
devnull = open(os.devnull, 'w')

def runScript(numThreads, angle):
    start = perf_counter_ns()
    result = subprocess.run(["./image_rotation", "img/200", "output/200", str(numThreads), str(angle)], stdout=devnull, stderr=devnull)
    end = perf_counter_ns()

    time = end-start
    return time

def clean():
    subprocess.run(['rm', '-rf', "output"])
    for i in range(11):
        subprocess.run(['mkdir', '-p', f"output/{i}"])
    subprocess.run(['mkdir', '-p', "output/200"])

def runTestCases(numIterations, numThreads):
    times = []
    for numThreads in numThreads:
        avg_time = 0
        for _ in range(numIterations):
            avg_time += runScript(numThreads, 180)
            clean()
        avg_time /= numIterations
        times.append(avg_time)

    return times

def plotBarChart(times, numThreads):
    plot.bar(numThreads, times, color = "blue")
    plot.xlabel("Number of Threads")
    plot.ylabel("Average Time (s)")
    plot.title("CPU Time vs. Number of Threads")
    plot.yscale("log")
    plot.tick_params(axis='both', which='both', direction='in', length=5, width=1, grid_color='gray', grid_alpha=0.5)
    plot.savefig(os.path.join(CWD, "executionTime.png"))

def main():
    numThreads = [1, 10, 50, 100]
    results = runTestCases(5, numThreads)
    print(f"Average time (s) for 1 thread: {results[0] / float(1e9)}")
    print(f"Average time (s) for 10 threads: {results[1] / float(1e9)}")
    print(f"Average time (s) for 50 threads: {results[2] / float(1e9)}")
    print(f"Average time (s) for 100 threads: {results[3] / float(1e9)}")
    devnull.close()
    
    plotBarChart(results, numThreads)
    print("Done!")

if __name__ == "__main__":
    main()
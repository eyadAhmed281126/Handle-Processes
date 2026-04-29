#pragma once
#include <vector>

struct Process {
    int pid;
    int arrivalTime;
    int burstTime;
};

struct GanttBlock {
    int pid;
    int start;
    int end;
};

struct ProcessResult {
    int pid;
    int arrivalTime;
    int burstTime;
    int completionTime;
    int turnaroundTime;
    int waitingTime;
    int responseTime;
};

struct SimResult {
    std::vector<GanttBlock>    gantt;
    std::vector<ProcessResult> results;
    double avgWT;
    double avgTAT;
    double avgRT;
};

SimResult SimulateRR(const std::vector<Process>& procs, int quantum);
SimResult SimulateSRTF(const std::vector<Process>& procs);

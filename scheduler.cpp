#include "scheduler.h"
#include <algorithm>
#include <numeric>
#include <queue>
#include <climits>

SimResult SimulateRR(const std::vector<Process>& procs, int quantum)
{
    SimResult res;
    res.avgWT = res.avgTAT = res.avgRT = 0.0;
    if (procs.empty()) return res;

    int n = (int)procs.size();
    std::vector<int> remaining(n);
    std::vector<int> firstRun(n, -1);
    std::vector<ProcessResult> results(n);

    for (int i = 0; i < n; i++) {
        remaining[i] = procs[i].burstTime;
        results[i].pid = procs[i].pid;
        results[i].arrivalTime = procs[i].arrivalTime;
        results[i].burstTime   = procs[i].burstTime;
        results[i].responseTime   = -1;
        results[i].completionTime = 0;
        results[i].turnaroundTime = 0;
        results[i].waitingTime    = 0;
    }

    std::queue<int> rq;
    std::vector<bool> inQueue(n, false);
    int time = 0;
    int done = 0;

    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return procs[a].arrivalTime < procs[b].arrivalTime;
    });

    int nextArrival = 0;
    auto enqueueArrivals = [&]() {
        while (nextArrival < n && procs[order[nextArrival]].arrivalTime <= time) {
            int idx = order[nextArrival];
            if (!inQueue[idx] && remaining[idx] > 0) {
                rq.push(idx);
                inQueue[idx] = true;
            }
            nextArrival++;
        }
    };

    enqueueArrivals();
    if (rq.empty() && nextArrival < n) {
        time = procs[order[nextArrival]].arrivalTime;
        enqueueArrivals();
    }

    while (done < n) {
        if (rq.empty()) {
            int nextAt = INT_MAX;
            for (int i = 0; i < n; i++)
                if (remaining[i] > 0)
                    nextAt = std::min(nextAt, procs[i].arrivalTime);
            if (nextAt == INT_MAX) break;
            res.gantt.push_back({-1, time, nextAt});
            time = nextAt;
            enqueueArrivals();
            continue;
        }

        int idx = rq.front();
        rq.pop();
        inQueue[idx] = false;

        if (firstRun[idx] == -1) {
            firstRun[idx] = time;
            results[idx].responseTime = time - procs[idx].arrivalTime;
        }

        int run = std::min(quantum, remaining[idx]);
        res.gantt.push_back({procs[idx].pid, time, time + run});
        time += run;
        remaining[idx] -= run;

        enqueueArrivals();

        if (remaining[idx] > 0) {
            rq.push(idx);
            inQueue[idx] = true;
        } else {
            results[idx].completionTime = time;
            results[idx].turnaroundTime = time - procs[idx].arrivalTime;
            results[idx].waitingTime    = results[idx].turnaroundTime - procs[idx].burstTime;
            done++;
        }
    }

    double sumWT = 0, sumTAT = 0, sumRT = 0;
    for (auto& r : results) {
        sumWT  += r.waitingTime;
        sumTAT += r.turnaroundTime;
        sumRT  += r.responseTime;
    }
    res.avgWT  = sumWT  / n;
    res.avgTAT = sumTAT / n;
    res.avgRT  = sumRT  / n;
    res.results = results;
    return res;
}

SimResult SimulateSRTF(const std::vector<Process>& procs)
{
    SimResult res;
    res.avgWT = res.avgTAT = res.avgRT = 0.0;
    if (procs.empty()) return res;

    int n = (int)procs.size();
    std::vector<int> remaining(n);
    std::vector<int> firstRun(n, -1);
    std::vector<ProcessResult> results(n);

    for (int i = 0; i < n; i++) {
        remaining[i] = procs[i].burstTime;
        results[i].pid = procs[i].pid;
        results[i].arrivalTime = procs[i].arrivalTime;
        results[i].burstTime   = procs[i].burstTime;
        results[i].responseTime   = -1;
        results[i].completionTime = 0;
        results[i].turnaroundTime = 0;
        results[i].waitingTime    = 0;
    }

    int time = 0, done = 0;
    int minAT = INT_MAX;
    for (auto& p : procs) minAT = std::min(minAT, p.arrivalTime);
    time = minAT;

    while (done < n) {
        int sel = -1, minR = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (remaining[i] > 0 && procs[i].arrivalTime <= time) {
                if (remaining[i] < minR ||
                    (sel != -1 && remaining[i] == minR &&
                     procs[i].arrivalTime < procs[sel].arrivalTime)) {
                    minR = remaining[i];
                    sel  = i;
                }
            }
        }

        if (sel == -1) {
            int nextAt = INT_MAX;
            for (int i = 0; i < n; i++)
                if (remaining[i] > 0) nextAt = std::min(nextAt, procs[i].arrivalTime);
            if (nextAt == INT_MAX) break;
            res.gantt.push_back({-1, time, nextAt});
            time = nextAt;
            continue;
        }

        if (firstRun[sel] == -1) {
            firstRun[sel] = time;
            results[sel].responseTime = time - procs[sel].arrivalTime;
        }

        int nextEvent = time + remaining[sel];
        for (int i = 0; i < n; i++) {
            if (remaining[i] > 0 && i != sel &&
                procs[i].arrivalTime > time && procs[i].arrivalTime < nextEvent)
                nextEvent = procs[i].arrivalTime;
        }

        if (!res.gantt.empty() && res.gantt.back().pid == procs[sel].pid &&
            res.gantt.back().end == time)
            res.gantt.back().end = nextEvent;
        else
            res.gantt.push_back({procs[sel].pid, time, nextEvent});

        remaining[sel] -= (nextEvent - time);
        time = nextEvent;

        if (remaining[sel] == 0) {
            results[sel].completionTime = time;
            results[sel].turnaroundTime = time - procs[sel].arrivalTime;
            results[sel].waitingTime    = results[sel].turnaroundTime - procs[sel].burstTime;
            done++;
        }
    }

    double sumWT = 0, sumTAT = 0, sumRT = 0;
    int cnt = 0;
    for (auto& r : results) {
        if (r.completionTime > 0) {
            sumWT  += r.waitingTime;
            sumTAT += r.turnaroundTime;
            sumRT  += r.responseTime;
            cnt++;
        }
    }
    if (cnt > 0) {
        res.avgWT  = sumWT  / cnt;
        res.avgTAT = sumTAT / cnt;
        res.avgRT  = sumRT  / cnt;
    }
    res.results = results;
    return res;
}

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>

using namespace std;

class ThreadClass
{
public:
    void Solve(int start, int iterationNum, double &partialSum)
    {
        partialSum = Calculate(start, iterationNum);
    };

    double Calculate(int start, int iterationNum)
    {
        double sum = 0.0;
        for (int i = start; i < start + iterationNum; i++)
            sum += (sqrt(i + 1) + i) / sqrt((i ^ 2) + i + 1);
        return sum;
    };
};

int main(int argc, const char **argv)
{
    ThreadClass threadObj;
    vector<thread> threads;
    int threadNum, m;
    double totalSum1 = 0.0;

    if (argc != 3 || sscanf(argv[2], "%d", &threadNum) != 1 || sscanf(argv[1], "%d", &m) != 1)
    {
        cout << "Usage: " << argv[0] << "number_of_thread\n"
             << flush;
        return 1;
    }

    vector<double> partialSums(threadNum);
    vector<int> threadIteration(threadNum, m / threadNum);
    int rest = m % threadNum;

    for (int i = 0; rest != 0; rest--, i++)
        threadIteration[i]++;

    vector<int> threadStart(threadNum, 0);

    for (size_t i = 1; i < threadStart.size(); i++)
        threadStart[i] = threadStart[i - 1] + threadIteration[i - 1];

    cout << "Main:         Start" << endl;

    auto start_time_single_thread = chrono::high_resolution_clock::now();
    double totalSum0 = threadObj.Calculate(0, m - 1);
    auto end_time_single_thread = chrono::high_resolution_clock::now();

    auto start_time_multi_thread = chrono::high_resolution_clock::now();

    for (int i = 0; i < threadNum; i++)
    {
        // cout << "Main:     Creating thread " << i << endl;
        threads.push_back(thread(&ThreadClass::Solve, &threadObj, threadStart[i], threadIteration[i], ref(partialSums[i])));
    }

    for (int i = 0; i < threadNum; i++)
        threads[i].join();

    for (const auto &partialSum : partialSums)
        totalSum1 += partialSum;

    auto end_time_multi_thread = chrono::high_resolution_clock::now();

    auto duration_single_thread = chrono::duration_cast<chrono::microseconds>(end_time_single_thread - start_time_single_thread);
    auto duration_multi_thread = chrono::duration_cast<chrono::microseconds>(end_time_multi_thread - start_time_multi_thread);

    double seconds_single_thread = duration_single_thread.count() / 1e6;
    double seconds_multi_thread = duration_multi_thread.count() / 1e6;

    cout << "Main:         Stop\n"
         << "Result:       " << totalSum1
         << "\nDuration:     " << setprecision(6) << seconds_multi_thread << " seconds"
         << "\nAcceleration: " << seconds_single_thread / seconds_multi_thread << endl;

    return 0;
}
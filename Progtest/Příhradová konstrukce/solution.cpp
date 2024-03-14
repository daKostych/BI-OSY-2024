#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>
#include <array>
#include <iterator>
#include <set>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <compare>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <condition_variable>
#include <pthread.h>
#include <semaphore.h>
#include "progtest_solver.h"
#include "sample_tester.h"
using namespace std;
#endif /* __PROGTEST__ */

struct Company
{
    Company(ACompany c, unsigned int id)
        : company(std::move(c)), companyID(id){};

    ACompany company;
    unsigned int companyID;
};

struct ProblemPack
{
    // ProblemPack() = default;
    ProblemPack(AProblemPack p, unsigned int cid, unsigned int pid)
        : problemPack(std::move(p)), companyID(cid), problemID(pid){};
    ProblemPack(unsigned int cid, unsigned int pid)
        : companyID(cid), problemID(pid) {}

    AProblemPack problemPack;
    unsigned int companyID = 0;
    unsigned int problemID = 0;
    unsigned int stopSignal = 0;
    bool firmLastProblem = false;
    bool threadStop = false;
    bool solvedMin = false;
    bool solvedCnt = false;

    // bool addToMinSolver = false;
    // bool addToCntSolver = false;
};

struct ProblemComparator
{
    bool operator()(const ProblemPack &p1, const ProblemPack &p2) { return p1.problemID > p2.problemID; };
};

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
class COptimizer
{
public:
    void receive(size_t companyID, int threadCnt);
    void solve(int tid);
    void send();


    static bool usingProgtestSolver(void)
    {
      return true;
    }
    static void checkAlgorithmMin(APolygon p)
    {
      // dummy implementation if usingProgtestSolver() returns true
    }
    static void checkAlgorithmCnt(APolygon p)
    {
      // dummy implementation if usingProgtestSolver() returns true
    }
    void start(int threadCount);
    void stop();
    void addCompany(ACompany company);

private:
    mutex mtx_UnsolvedQueue, mtx_SolvedPacks, mtx_MinSolver, mtx_CntSolver;
    condition_variable cv_EmptyUnsolvedQueue, cv_EmptySolvedPacks;
    unsigned int unsolvedFirms = 0 /*, threadCnt = 0*/;
    vector<Company> companyList;
    queue<shared_ptr<ProblemPack>> unsolvedQueue;
    vector<queue<shared_ptr<ProblemPack>>> solvedPacks;
    vector<thread> workingThreads;
    vector<thread> receivingThreads;
    vector<thread> handoverThreads;
    AProgtestSolver minSolver = createProgtestMinSolver();
    AProgtestSolver cntSolver = createProgtestCntSolver();
    vector<shared_ptr<ProblemPack>> addToMinSolver;
    vector<shared_ptr<ProblemPack>> addToCntSolver;
};

void COptimizer::start(int threadCount)
{
    // threadCnt = threadCount;
    for (size_t i = 0; i < companyList.size(); i++)
    {
        receivingThreads.emplace_back(&COptimizer::receive, this, i, threadCount);
        handoverThreads.emplace_back(&COptimizer::send, this);
    }

    for (int i = 0; i < threadCount; i++)
        workingThreads.emplace_back(&COptimizer::solve, this, i);
}

void COptimizer::stop()
{
    for (auto &t : receivingThreads)
        t.join();

    for (auto &t : workingThreads)
        t.join();

    for (auto &t : handoverThreads)
        t.join();
}

void COptimizer::addCompany(ACompany company)
{
    mtx_UnsolvedQueue.lock();
    mtx_SolvedPacks.lock();

    if (solvedPacks.size() == solvedPacks.capacity())
        solvedPacks.resize((solvedPacks.capacity() ^ 2) + 10);

    unsolvedFirms++;
    if (companyList.empty())
        companyList.emplace_back(company, 0);
    else
        companyList.emplace_back(company, companyList.back().companyID + 1);

    mtx_UnsolvedQueue.unlock();
    mtx_SolvedPacks.unlock();
}

void COptimizer::receive(size_t companyID, int threadCnt)
{
}

void COptimizer::send()
{
}

void COptimizer::solve(int tid)
{
}
// TODO: COptimizer implementation goes here
//-------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__
int main(void)
{
  COptimizer optimizer;
  ACompanyTest company = std::make_shared<CCompanyTest>();
  optimizer.addCompany(company);
  optimizer.start(4);
  optimizer.stop();
  if (!company->allProcessed())
    throw std::logic_error("(some) problems were not correctly processsed");
  return 0;
}
#endif /* __PROGTEST__ */

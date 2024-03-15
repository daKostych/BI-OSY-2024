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
    ProblemPack(AProblemPack p, unsigned int cid, unsigned int pid)
        : problemPack(std::move(p)), companyID(cid), problemID(pid){};
    ProblemPack(unsigned int cid, unsigned int pid)
        : companyID(cid), problemID(pid) {};
    explicit ProblemPack(bool stop)
        : threadStop(stop) { stopSignal = 0; };

    AProblemPack problemPack = nullptr;
    unsigned int companyID = 0;
    unsigned int problemID = 0;
    unsigned int stopSignal = 0;
    bool firmLastProblem = false;
    bool threadStop = false;
    bool solvedMin = false;
    bool solvedCnt = false;
};

struct ProblemComparator
{
    bool operator()(const shared_ptr<ProblemPack>& p1, const shared_ptr<ProblemPack>& p2) { return p1->problemID > p2->problemID; };
};

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
class COptimizer
{
public:
    void receive(size_t companyID, int threadCnt);
    void solve(int tid);
    void send(size_t companyID);

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
    unsigned int unsolvedFirms = 0, threadCnt = 0;
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
    threadCnt = threadCount;
    for (size_t i = 0; i < companyList.size(); i++)
    {
        receivingThreads.emplace_back(&COptimizer::receive, this, i, threadCount);
        handoverThreads.emplace_back(&COptimizer::send, this, i);
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
    for (unsigned int i = 0; ; i++)
    {
        AProblemPack problem = companyList[companyID].company->waitForPack();
        shared_ptr<ProblemPack> newProblem;

        if(problem.get() != nullptr)
        {
            newProblem = make_shared<ProblemPack>(problem, companyList[companyID].companyID, i);
            mtx_UnsolvedQueue.lock();

            //cout << "Receiving thread: Add problem number " << newProblem->problemID << endl;
            unsolvedQueue.push(newProblem);
            cv_EmptyUnsolvedQueue.notify_one();

            mtx_UnsolvedQueue.unlock();
        }
        else
        {
            shared_ptr<ProblemPack> firmEnd = make_shared<ProblemPack>(companyList[companyID].companyID, i);
            firmEnd->firmLastProblem = true;
            mtx_UnsolvedQueue.lock();

            //cout << "Receiving thread: Add the last problem of the firm to unsolved Queue" << endl;
            unsolvedQueue.push(firmEnd);

            if(unsolvedFirms == 1)
            {
                for (int j = 0; j < threadCnt; j++)
                {
                    shared_ptr<ProblemPack> workerStop = make_shared<ProblemPack>(true);
                    workerStop->stopSignal = j;
                    //cout << "Receiving thread: Add stop signal to work thread number " << j << endl;
                    unsolvedQueue.push(workerStop);
                }
            }
            unsolvedFirms--;
            cv_EmptyUnsolvedQueue.notify_all();

            mtx_UnsolvedQueue.unlock();
            break;
        }
    }
}

void COptimizer::send(size_t  companyID)
{
    unsigned int sendNow = 0, max = UINT_MAX;
    priority_queue<shared_ptr<ProblemPack>, vector<shared_ptr<ProblemPack>>, ProblemComparator> q;

    while(true)
    {
        if(sendNow == max)
            break;

        unique_lock<mutex> lock(mtx_SolvedPacks);

        cv_EmptySolvedPacks.wait(lock, [&]()
                                { return !(solvedPacks[companyID].empty()); });

        if(solvedPacks[companyID].empty())
        {
            lock.unlock();
            continue;
        }

        shared_ptr<ProblemPack> solvedPack = solvedPacks[companyID].front();
        //cout << "Handover thread: Get pack number " << solvedPack->problemID << endl;
        solvedPacks[companyID].pop();

        lock.unlock();

        if(solvedPack->firmLastProblem)
            max = solvedPack->problemID;

        if(solvedPack->problemID == sendNow)
        {
            if(solvedPack->problemID < max)
            {
                //cout << "Handover thread: Send back pack number " << solvedPack->problemID << " of " << max - 1 << endl;
                companyList[companyID].company->solvedPack(solvedPack->problemPack);
                sendNow++;

                while ((!q.empty()) && (sendNow == q.top()->problemID))
                {
                    if (q.top()->problemID == max)
                    {
                        //cout << "Handover thread: Sent all packs of the firm" << endl;
                        q.pop();
                        break;
                    }
                    //cout << "Handover thread: Send back pack number: " << q.top()->problemID << " of " << max - 1 << endl;
                    companyList[companyID].company->solvedPack(q.top()->problemPack);
                    sendNow++;
                    q.pop();
                }
            }
        }
        else
            q.push(solvedPack);
    }
}

void COptimizer::solve(int tid)
{
    vector<shared_ptr<ProblemPack>> problems;

    while(true)
    {
        unique_lock<mutex> lock_UnsolvedQueue(mtx_UnsolvedQueue);

        cv_EmptyUnsolvedQueue.wait(lock_UnsolvedQueue, [&]()
                                   { return !unsolvedQueue.empty(); });

        shared_ptr<ProblemPack> toSolve = unsolvedQueue.front();
        unsolvedQueue.pop();

        lock_UnsolvedQueue.unlock();

        if (toSolve->threadStop)
        {
            //cout << "Working thread number " << tid << ": Get pack with stop signal number " << toSolve->stopSignal << endl;
            if (toSolve->stopSignal == threadCnt - 1)
            {
                mtx_MinSolver.lock();
                mtx_CntSolver.lock();

                //cout << "Work thread number: " << tid << " Start last solve process" << endl;
                minSolver->solve();
                cntSolver->solve();

                for(auto & j : addToMinSolver)
                    j->solvedMin = true;
                for(auto & j : addToCntSolver)
                    j->solvedCnt = true;

                mtx_MinSolver.unlock();
                mtx_CntSolver.unlock();
            }

            if (problems.empty())
                break;

            else
            {
                while (!problems.empty())
                {
                    unique_lock<mutex> lock_SolvedPacks(mtx_SolvedPacks);
                    if(problems[0]->solvedMin && problems[0]->solvedCnt)
                    {
                    //cout << "Work thread number " << tid << ": Send solved pack number " << problems[0]->problemID << endl;
                    solvedPacks[problems[0]->companyID].push(problems[0]);
                    cv_EmptySolvedPacks.notify_all();
                    problems.erase(problems.begin());
                    }
                }
                break;
            }
        }

        if (toSolve->firmLastProblem)
        {
            unique_lock<mutex> lock_SolvedPacks(mtx_SolvedPacks);
            //cout << "Work thread number " << tid << ": Send last solved pack of the company " << toSolve->problemID << endl;
            solvedPacks[toSolve->companyID].push(toSolve);
            cv_EmptySolvedPacks.notify_all();
            continue;
        }

        problems.emplace_back(toSolve);

        if(toSolve->problemPack->m_ProblemsMin.empty())
        {
            unique_lock<mutex> lock_MinSolver(mtx_MinSolver);
            addToMinSolver.emplace_back(toSolve);
        }
        for (size_t i = 0; i < toSolve->problemPack->m_ProblemsMin.size(); i++)
        {
            unique_lock<mutex> lock_MinSolver(mtx_MinSolver);
            if (minSolver->hasFreeCapacity())
            {
                if (i == toSolve->problemPack->m_ProblemsMin.size() - 1)
                    addToMinSolver.emplace_back(toSolve);
                minSolver->addPolygon(toSolve->problemPack->m_ProblemsMin[i]);
                if(!minSolver->hasFreeCapacity())
                {
                    AProgtestSolver copyMinSolver = minSolver;
                    minSolver = createProgtestMinSolver();
                    vector<shared_ptr<ProblemPack>> copyAddToMinSolver = addToMinSolver;
                    addToMinSolver.clear();
                    lock_MinSolver.unlock();

                    copyMinSolver->solve();
                    for(auto & j : copyAddToMinSolver)
                        j->solvedMin = true;
                }
            }
        }

        if(toSolve->problemPack->m_ProblemsCnt.empty())
        {
            unique_lock<mutex> lock_CntSolver(mtx_CntSolver);
            addToCntSolver.emplace_back(toSolve);
        }
        for (size_t i = 0; i < toSolve->problemPack->m_ProblemsCnt.size(); i++)
        {
            unique_lock<mutex> lock_CntSolver(mtx_CntSolver);
            if (cntSolver->hasFreeCapacity())
            {
                if (i == toSolve->problemPack->m_ProblemsCnt.size() - 1)
                    addToCntSolver.emplace_back(toSolve);
                cntSolver->addPolygon(toSolve->problemPack->m_ProblemsCnt[i]);
                if (!cntSolver->hasFreeCapacity())
                {
                    AProgtestSolver copyCntSolver = cntSolver;
                    cntSolver = createProgtestCntSolver();
                    vector<shared_ptr<ProblemPack>> copyAddToCntSolver = addToCntSolver;
                    addToCntSolver.clear();
                    lock_CntSolver.unlock();

                    copyCntSolver->solve();
                    for(auto & j : copyAddToCntSolver)
                        j->solvedCnt = true;
                }
            }
        }

        unique_lock<mutex> lock_SolvedPacks(mtx_SolvedPacks);
        for (size_t j = 0; j < problems.size(); j++)
        {
            if (problems[j]->solvedMin && problems[j]->solvedCnt)
            {
                //cout << "Work thread number " << tid << ": Send solved pack number " << problems[j]->problemID << endl;
                solvedPacks[problems[j]->companyID].push(problems[j]);
                problems.erase(problems.begin() + ((int)j));
                j--;
            }
        }
        cv_EmptySolvedPacks.notify_all();
        lock_SolvedPacks.unlock();
    }
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

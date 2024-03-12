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
      : company(c), companyID(id){};

  ACompany company;
  unsigned int companyID;
  /*
  thread receivingThread;
  thread handoverThread;
  */
};

struct ProblemPack
{
  ProblemPack() = default;
  ProblemPack(AProblemPack p, unsigned int cid, unsigned int pid)
      : problemPack(p), companyID(cid), problemID(pid){};
  ProblemPack(unsigned int cid, unsigned int pid) : companyID(cid), problemID(pid) {}

  AProblemPack problemPack;
  unsigned int companyID;
  unsigned int problemID;
  unsigned int stopSignal = 0;
  bool firmLastProblem = false;
  bool threadStop = false;
  bool solvedMin = false;
  bool addToMinSolver = false;
  bool solvedCnt = false;
  bool addToCntSolver = false;
};

struct ProblemCamparator
{
  bool operator()(const ProblemPack &p1, const ProblemPack &p2) { return p1.problemID > p2.problemID; };
};

void receive(Company &company, queue<ProblemPack> &unsolvedQueue, mutex &mtx, condition_variable &cv_empty_unsolved_queue, unsigned int &unsolvedFirms, int threadCount)
{
  for (unsigned int i = 0;; i++)
  {
    AProblemPack problem = company.company->waitForPack();
    ProblemPack newProblem;

    if (problem)
      newProblem = ProblemPack(problem, company.companyID, i);
    else
      newProblem = ProblemPack(company.companyID, i);

    mtx.lock();

    if (!problem)
    {
      newProblem.firmLastProblem = true;
      cout << "Add the last problem of the firm to unsolved Queue" << endl;
      unsolvedQueue.push(newProblem);
      if (unsolvedFirms == 1)
      {
        newProblem.threadStop = true;
        for (int i = 0; i < threadCount; i++)
        {
          newProblem.stopSignal = i;
          cout << "Add stop signal to work thread number: " << i << endl;
          unsolvedQueue.push(newProblem);
        }
      }
      unsolvedFirms--;
      cv_empty_unsolved_queue.notify_all();
      mtx.unlock();
      break;
    }
    else
    {
      cout << "Add problem number: " << newProblem.problemID << endl;
      unsolvedQueue.push(newProblem);
      cv_empty_unsolved_queue.notify_one();
      mtx.unlock();
    }
  }
};

void send(Company &company, vector<queue<ProblemPack>> &solvedPacks, mutex &mtx, condition_variable &cv_empty_solved_packs)
{
  unsigned int sendNow = 0, max = UINT_MAX;
  priority_queue<ProblemPack, vector<ProblemPack>, ProblemCamparator> q;
  // unique_lock<mutex> lock(mtx);

  while (true)
  {
    unique_lock<mutex> lock(mtx);

    cv_empty_solved_packs.wait(lock, [&solvedPacks, &company]()
                               { return !solvedPacks[company.companyID].empty(); });

    if (solvedPacks[company.companyID].empty())
    {
      lock.unlock();
      continue;
    }

    // mtx.lock();

    ProblemPack solvedPack = solvedPacks[company.companyID].front();
    cout << "Handover thread get pack number: " << solvedPack.problemID << endl;
    solvedPacks[company.companyID].pop();

    lock.unlock();

    if (solvedPack.firmLastProblem)
      max = solvedPack.problemID;

    if (sendNow == max)
      break;

    if (solvedPack.problemID == sendNow)
    {
      if (solvedPack.problemID < max)
      {
        cout << "Send back pack number: " << solvedPack.problemID << " of " << max - 1 << endl;
        company.company->solvedPack(solvedPack.problemPack);
        sendNow++;
      }

      while ((!q.empty()) && (sendNow == q.top().problemID))
      {
        cout << "Send back pack number: " << q.top().problemID << " of " << max - 1 << endl;
        company.company->solvedPack(q.top().problemPack);
        sendNow++;
        q.pop();
      }
    }
    else
      q.push(solvedPack);
  }
};

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
class COptimizer
{
public:
  COptimizer() = default;
  // ~COptimizer() { solvedPacks.clear(); };
  // COptimizer operator=(){};

  void solve(int tid);

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

  void start(int threadCount)
  {
    threadCnt = threadCount;
    for (size_t i = 0; i < companyList.size(); i++)
    {
      receivingThreads.emplace_back(thread(receive, ref(companyList[i]), ref(unsolvedQueue), ref(mtx1), ref(cv_empty_unsolved_queue), ref(unsolvedFirms), threadCount));
      handoverThreads.emplace_back(thread(send, ref(companyList[i]), ref(solvedPacks), ref(mtx2), ref(cv_empty_solved_packs)));
    }

    for (int i = 0; i < threadCount; i++)
    {
      workingThreads.emplace_back(thread(&COptimizer::solve, this, i));
    }
  };

  void stop(void)
  {
    for (auto &t : receivingThreads)
      t.join();

    for (auto &t : workingThreads)
      t.join();

    for (auto &t : handoverThreads)
      t.join();
  };

  void addCompany(ACompany company)
  {
    mtx1.lock();
    mtx2.lock();

    if (solvedPacks.size() == solvedPacks.capacity())
      solvedPacks.resize((solvedPacks.capacity() ^ 2) + 10);

    unsolvedFirms++;
    if (companyList.empty())
      companyList.emplace_back(Company(company, 0));
    else
      companyList.emplace_back(Company(company, companyList.back().companyID + 1));

    mtx1.unlock();
    mtx2.unlock();
  };

private:
  mutex mtx1, mtx2, mtx_MinSolver, mtx_CntSolver;
  condition_variable cv_empty_unsolved_queue, cv_empty_solved_packs;
  unsigned int unsolvedFirms = 0, threadCnt = 0;
  vector<Company> companyList;
  queue<ProblemPack> unsolvedQueue;
  vector<queue<ProblemPack>> solvedPacks;
  vector<thread> workingThreads;
  vector<thread> receivingThreads;
  vector<thread> handoverThreads;
  AProgtestSolver minSolver = createProgtestMinSolver();
  AProgtestSolver cntSolver = createProgtestCntSolver();
};

void COptimizer::solve(int tid)
{
  // AProgtestSolver minSolver = createProgtestMinSolver();
  // AProgtestSolver cntSolver = createProgtestCntSolver();

  vector<ProblemPack> problems;
  // ProblemPack toSolve;

  while (true)
  {
    unique_lock<mutex> lock1(mtx1);

    cv_empty_unsolved_queue.wait(lock1, [&]()
                                 { return !unsolvedQueue.empty(); });

    ProblemPack toSolve = unsolvedQueue.front();
    unsolvedQueue.pop();

    lock1.unlock();

    if (toSolve.threadStop)
    {
      if (toSolve.stopSignal == 0)
      {
        mtx_MinSolver.lock();
        mtx_CntSolver.lock();

        minSolver->solve();
        cntSolver->solve();

        mtx_MinSolver.unlock();
        mtx_CntSolver.unlock();
      }

      if (problems.empty())
        break;

      else
      {
        mtx_MinSolver.lock();
        mtx_CntSolver.lock();
        unique_lock<mutex> lock2(mtx2);
        while (!problems.empty())
        {
          problems[0].solvedMin = true;
          problems[0].solvedCnt = true;
          cout << "Work thread number " << tid << " send solved pack number: " << problems[0].problemID << endl;
          solvedPacks[problems[0].companyID].push(problems[0]);
          cv_empty_solved_packs.notify_all();
          problems.erase(problems.begin());
        }
        lock2.unlock();
        mtx_MinSolver.unlock();
        mtx_CntSolver.unlock();
        break;
      }
    }

    if (toSolve.firmLastProblem)
    {
      unique_lock<mutex> lock2(mtx2);
      cout << "Work thread number " << tid << " send last solved pack of the company: " << toSolve.problemID << endl;
      solvedPacks[toSolve.companyID].push(toSolve);
      cv_empty_solved_packs.notify_all();
      continue;
    }

    problems.emplace_back(toSolve);

    for (size_t i = 0; i < toSolve.problemPack->m_ProblemsMin.size(); i++)
    {
      unique_lock<mutex> lockMinSolver(mtx_MinSolver);
      if (minSolver->hasFreeCapacity())
      {
        if (i == toSolve.problemPack->m_ProblemsMin.size() - 1)
        {
          problems.back().addToMinSolver = true;
          minSolver->addPolygon(toSolve.problemPack->m_ProblemsMin[i]);
          // lockMinSolver.unlock();
        }
        else
          minSolver->addPolygon(toSolve.problemPack->m_ProblemsMin[i]);
      }
      else
      {
        AProgtestSolver copySolver = minSolver;
        minSolver = createProgtestMinSolver();
        lockMinSolver.unlock();

        for (size_t j = 0; j < problems.size(); j++)
          if (problems[j].addToMinSolver == true)
            problems[j].solvedMin = true;

        copySolver->solve();
      }
    }

    for (size_t i = 0; i < toSolve.problemPack->m_ProblemsCnt.size(); i++)
    {
      unique_lock<mutex> lockCntSolver(mtx_CntSolver);
      if (cntSolver->hasFreeCapacity())
      {
        if (i == toSolve.problemPack->m_ProblemsCnt.size() - 1)
        {
          problems.back().addToCntSolver = true;
          cntSolver->addPolygon(toSolve.problemPack->m_ProblemsCnt[i]);
          // lockCntSolver.unlock();
        }
        else
          cntSolver->addPolygon(toSolve.problemPack->m_ProblemsCnt[i]);
      }
      else
      {
        AProgtestSolver copySolver = cntSolver;
        cntSolver = createProgtestCntSolver();
        lockCntSolver.unlock();

        for (size_t j = 0; j < problems.size(); j++)
          if (problems[j].addToCntSolver == true)
            problems[j].solvedCnt = true;

        copySolver->solve();
      }
    }

    unique_lock<mutex> lock2(mtx2);
    for (size_t j = 0; j < problems.size(); j++)
    {
      if ((problems[j].solvedMin == true) && (problems[j].solvedCnt == true))
      {
        cout << "Work thread number " << tid << " send solved pack number: " << problems[j].problemID << endl;
        solvedPacks[problems[j].companyID].push(problems[j]);
        cv_empty_solved_packs.notify_all();
        problems.erase(problems.begin() + j);
        j--;
      }
    }
    lock2.unlock();
  }
};
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

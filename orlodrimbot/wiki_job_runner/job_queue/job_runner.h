#ifndef JOB_RUNNER_H
#define JOB_RUNNER_H

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/json.h"
#include "job_queue.h"

namespace job_queue {

constexpr cbl::DateDiff INFINITE_DELAY = cbl::DateDiff::fromYears(1000);

enum class ErrorLevel {
  INFO,
  WARNING,
  ERROR,
};

class JobExecutionError : public cbl::Error {
public:
  JobExecutionError(ErrorLevel errorLevel, const std::string& description, json::Value&& structuredInfo,
                    std::optional<int> newPriority = std::nullopt, const cbl::DateDiff& minRetryDelay = cbl::DateDiff())
      : Error(description), m_errorLevel(errorLevel), m_structuredInfo(std::move(structuredInfo)),
        m_newPriority(newPriority), m_minRetryDelay(minRetryDelay) {}
  JobExecutionError(ErrorLevel errorLevel, const std::string& description, const std::string& source,
                    const std::string& code, std::optional<int> newPriority = std::nullopt,
                    const cbl::DateDiff& minRetryDelay = cbl::DateDiff());

  ErrorLevel errorLevel() const { return m_errorLevel; }
  const json::Value& structuredInfo() const { return m_structuredInfo; }
  std::optional<int> newPriority() const { return m_newPriority; }
  cbl::DateDiff minRetryDelay() const { return m_minRetryDelay; }

private:
  ErrorLevel m_errorLevel;
  json::Value m_structuredInfo;
  std::optional<int> m_newPriority;
  cbl::DateDiff m_minRetryDelay;
};

class JobHandler {
public:
  virtual ~JobHandler() = default;
  virtual void startBatch(const std::vector<Job>& jobs, JobQueue& jobQueue){};
  virtual void run(const Job& job, JobQueue& jobQueue){};
  virtual void run(const Job& job, JobQueue& jobQueue, bool dryRun);
  virtual void endBatch(JobQueue& jobQueue){};
  virtual int maxBatchSize() const { return 1; }
};

using JobHandlers = std::unordered_map<std::string, JobHandler*>;

struct RunJobOptions {
  int maxCount = 10;
  double backoffRandomness = 0.5;
  bool dryRun = false;
};

void runJobs(JobQueue& jobQueue, const JobHandlers& jobHandlers, const RunJobOptions& options = {});

cbl::DateDiff randomizeDelay(cbl::DateDiff delay, double randomness);

}  // namespace job_queue

#endif

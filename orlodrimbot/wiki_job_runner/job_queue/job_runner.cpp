#include "job_runner.h"
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/random.h"
#include "job_queue.h"

using cbl::Date;
using cbl::DateDiff;
using std::optional;
using std::string;
using std::vector;

namespace job_queue {
namespace {

JobQueue::Rescheduling rescheduleHelper(const Job& job, json::Value&& result, json::Value& attempts,
                                        const DateDiff& minRetryDelay, optional<int> priority,
                                        const RunJobOptions& options) {
  json::Value& countVal = attempts.getMutable("count");
  countVal = countVal.numberAsInt64() + 1;
  json::Value& retryDelayVal = attempts.getMutable("retryDelay");
  Date rescheduleDate;
  if (minRetryDelay != INFINITE_DELAY) {
    DateDiff retryDelayLowerBound = std::max(minRetryDelay, DateDiff::fromMinutes(3));
    DateDiff doubledOldRetryDelay =
        std::min(DateDiff::fromSeconds(retryDelayVal.numberAsInt64() * 2), DateDiff::fromDays(200));
    DateDiff newRetryDelay =
        randomizeDelay(std::max(retryDelayLowerBound, doubledOldRetryDelay), options.backoffRandomness);
    retryDelayVal = newRetryDelay.seconds();
    rescheduleDate = Date::now() + newRetryDelay;
  } else {
    retryDelayVal = "infinite";
    rescheduleDate = Date(3000, 1, 1, 0, 0, 0);
  }
  return {.jobId = job.id, .priority = priority, .date = rescheduleDate, .result = std::move(result)};
}

JobQueue::Rescheduling rescheduleBeforeTrying(const Job& job, const RunJobOptions& options) {
  json::Value result = job.lastAttemptResult.copy();
  return rescheduleHelper(job, std::move(result), result.getMutable("unfinishedAttempts"), DateDiff(), std::nullopt,
                          options);
}

void rescheduleAfterFailure(JobQueue& jobQueue, const Job& job, const JobExecutionError& error,
                            const RunJobOptions& options) {
  if (options.dryRun) {
    return;
  }
  json::Value result = job.lastAttemptResult.copy();
  result.erase("unfinishedAttempts");
  result.getMutable("failure") = error.structuredInfo().copy();
  vector<JobQueue::Rescheduling> reschedulings;
  reschedulings.push_back(rescheduleHelper(job, std::move(result), result.getMutable("failedAttempts"),
                                           error.minRetryDelay(), error.newPriority(), options));
  jobQueue.rescheduleJobs(reschedulings);
}

string getTopLevelHandler(const Job& job) {
  return job.handler.substr(0, job.handler.find('.'));
}

void runOneBatchOfJobs(JobQueue& jobQueue, const JobHandlers& jobHandlers, const RunJobOptions& options,
                       int& maxCount) {
  vector<Job> jobs;
  JobHandler* handler = nullptr;
  int handlerBatchSize = 1000;  // Default for an invalid handler.
  int skipped = 0;
  // Get the first job in the queue and more jobs with the same priority and handler, if possible.
  jobQueue.enumerateJobsToRun([&](Job& job) {
    if (jobs.empty()) {
      JobHandlers::const_iterator jobHandlerIt = jobHandlers.find(getTopLevelHandler(job));
      if (jobHandlerIt != jobHandlers.end()) {
        handler = jobHandlerIt->second;
        handlerBatchSize = handler->maxBatchSize();
      }
    } else {
      if (job.priority != jobs[0].priority) {
        return false;
      } else if (job.handler != jobs[0].handler) {
        skipped++;
        return skipped < 10;
      }
    }
    jobs.push_back(std::move(job));
    maxCount--;
    skipped = 0;
    return maxCount > 0 && static_cast<int>(jobs.size()) < handlerBatchSize;
  });

  if (jobs.empty()) {
    return;
  } else if (handler == nullptr) {
    for (const Job& job : jobs) {
      CBL_ERROR << "Job with invalid handler: " << job.debugString();
      if (!options.dryRun) {
        jobQueue.removeJob(job.id);
      }
    }
    return;
  }

  // Reschedule jobs before trying to run them to avoid getting stuck in a crash loop.
  if (!options.dryRun) {
    vector<JobQueue::Rescheduling> reschedulings;
    reschedulings.reserve(jobs.size());
    for (const Job& job : jobs) {
      reschedulings.push_back(rescheduleBeforeTrying(job, options));
    }
    jobQueue.rescheduleJobs(reschedulings);
  }

  vector<Job> delayedJobs;
  while (true) {
    try {
      handler->startBatch(jobs, jobQueue);
      break;
    } catch (const JobExecutionError& error) {
      if (jobs.size() == 1) {
        // If the preparation is for a single job, reschedule the job as if run() has failed.
        CBL_ERROR << "Failed to process job " << jobs[0].debugString() << ": " << error.what();
        rescheduleAfterFailure(jobQueue, jobs[0], error, options);
        return;
      }
      // Since we cannot know which job caused the failure, split the batch into two halves and try again.
      CBL_WARNING << "Splitting batch of size " << jobs.size() << " for handler " << getTopLevelHandler(jobs[0])
                  << " after error: " << error.what();
      int middle = jobs.size() / 2;
      CBL_ASSERT(middle > 0);
      // Cancel the previous rescheduling for the jobs that we will not process now.
      if (!options.dryRun) {
        vector<JobQueue::Rescheduling> reschedulings;
        reschedulings.reserve(jobs.size() - middle);
        for (int i = middle; i < static_cast<int>(jobs.size()); i++) {
          reschedulings.push_back({.jobId = jobs[i].id, .date = Date(), .result = jobs[i].lastAttemptResult.copy()});
        }
        jobQueue.rescheduleJobs(reschedulings);
      }
      jobs.resize(jobs.size() / 2);
    }
  }

  for (const Job& job : jobs) {
    try {
      CBL_INFO << "Running job " << job.debugString();
      handler->run(job, jobQueue, options.dryRun);
      if (!options.dryRun) {
        jobQueue.removeJob(job.id);
      }
    } catch (const JobExecutionError& error) {
      switch (error.errorLevel()) {
        case ErrorLevel::INFO:
          CBL_INFO << "Rescheduling job " << job.debugString() << ": " << error.what();
          break;
        case ErrorLevel::WARNING:
          CBL_WARNING << "Rescheduling job " << job.debugString() << ": " << error.what();
          break;
        case ErrorLevel::ERROR:
          CBL_ERROR << "Job " << job.debugString() << " failed: " << error.what();
          break;
      }
      rescheduleAfterFailure(jobQueue, job, error, options);
    }
  }

  handler->endBatch(jobQueue);
}

}  // namespace

JobExecutionError::JobExecutionError(ErrorLevel errorLevel, const std::string& description, const string& source,
                                     const string& code, optional<int> newPriority, const DateDiff& minRetryDelay)
    : Error(description), m_errorLevel(errorLevel), m_newPriority(newPriority), m_minRetryDelay(minRetryDelay) {
  json::Value& sourceError = m_structuredInfo.getMutable(source + "Error");
  sourceError.getMutable("code") = code;
  sourceError.getMutable("description") = description;
}

void JobHandler::run(const Job& job, JobQueue& jobQueue, bool dryRun) {
  if (dryRun) {
    CBL_INFO << "[DRY RUN] Running job " << job.debugString();
  } else {
    run(job, jobQueue);
  }
}

void runJobs(JobQueue& jobQueue, const JobHandlers& jobHandlers, const RunJobOptions& options) {
  int maxCount = options.maxCount;
  while (true) {
    if (maxCount <= 0) {
      CBL_INFO << "Maximum number of queue reads reached, exiting";
      break;
    }
    Date firstJobDate = jobQueue.getFirstJobDate();
    if (firstJobDate.isNull()) {
      CBL_INFO << "No job left in the queue, exiting";
      break;
    }
    DateDiff timeToWait = firstJobDate - Date::now();
    if (timeToWait.seconds() > 0) {
      CBL_INFO << "Next job in " << timeToWait.seconds() << " seconds, exiting";
      break;
    }
    runOneBatchOfJobs(jobQueue, jobHandlers, options, maxCount);
  }
}

DateDiff randomizeDelay(DateDiff delay, double randomness) {
  return DateDiff::fromSeconds(static_cast<int>(delay.seconds() * (1 + cbl::randomDouble(randomness))));
}

}  // namespace job_queue

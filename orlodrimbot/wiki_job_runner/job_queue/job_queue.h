#ifndef JOB_QUEUE_H
#define JOB_QUEUE_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/sqlite.h"

namespace job_queue {

using JobId = int64_t;
constexpr JobId INVALID_JOB_ID = -1;

struct Job {
  JobId id = INVALID_JOB_ID;
  std::string handler;
  std::string key;
  int priority = 0;
  cbl::Date insertedOn;
  cbl::Date runAfter;
  json::Value parameters;
  // Those fields are ignored by JobQueue::insertJob.
  cbl::Date lastAttempt;
  json::Value lastAttemptResult;

  std::string debugString() const;
};

enum InsertMode {
  IGNORE_DUPS,
  OVERWRITE_IF_EXISTS,
  IGNORE_IF_EXISTS,
};

class JobQueue {
public:
  explicit JobQueue(const std::string& databasePath);
  JobQueue(const JobQueue&) = delete;
  JobQueue& operator=(const JobQueue&) = delete;

  JobId insertJob(const Job& job, InsertMode insertMode = IGNORE_DUPS);
  void insertJobs(const std::vector<Job>& jobs, InsertMode insertMode = IGNORE_DUPS);
  void removeJob(JobId jobId);
  void removeJobs(const std::vector<JobId>& jobIds);
  struct Rescheduling {
    JobId jobId;
    std::optional<int> priority;
    cbl::Date date;
    json::Value result;
  };
  void rescheduleJobs(const std::vector<Rescheduling>& reschedulings);
  void enumerateAllJobs(const std::function<void(Job& job)>& callback);
  void enumerateJobsToRun(const std::function<bool(Job& job)>& callback);
  void enumerateJobsByHandler(const std::string& handler, const std::function<void(Job& job)>& callback);
  std::vector<Job> getJobsByHandlerAndKey(const std::string& handler, const std::string& key);

  cbl::Date getFirstJobDate();

private:
  JobId insertJobUnlocked(const Job& job, InsertMode insertMode);
  void updateStartFromMostRecent(JobId removedJobId);
  void enumerateJobsFromStatement(sqlite::Statement&& statement, const std::function<void(Job& job)>& callback);

  sqlite::Database m_database;
};

}  // namespace job_queue

#endif

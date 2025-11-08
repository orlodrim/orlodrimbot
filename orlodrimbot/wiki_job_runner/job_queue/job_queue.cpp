#include "job_queue.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/sqlite.h"
#include "cbl/string.h"

using cbl::Date;
using sqlite::Database;
using sqlite::Statement;
using std::optional;
using std::string;
using std::vector;

namespace job_queue {
namespace {

constexpr char START_MOST_MOST_RECENT[] = "start_from_most_recent";

constexpr char BASE_JOB_SELECT[] =
    "SELECT id, handler, key, priority, inserted_on, run_after, parameters, last_attempt, last_attempt_result "
    "FROM job ";

json::Value parseOptionalJson(const char* str) {
  return *str == '\0' ? json::Value() : json::parse(str);
}

Date parseOptionalDate(int64_t timestamp) {
  return timestamp == 0 ? Date() : Date::fromTimeT(timestamp);
}

void initJobFromStatement(Job& job, Statement& statement) {
  job.id = statement.columnInt64(0);
  job.handler = statement.columnTextNotNull(1);
  job.key = statement.columnTextNotNull(2);
  job.priority = statement.columnInt(3);
  job.insertedOn = Date::fromTimeT(statement.columnInt64(4));
  job.runAfter = parseOptionalDate(statement.columnInt64(5));
  job.parameters = parseOptionalJson(statement.columnTextNotNull(6));
  job.lastAttempt = parseOptionalDate(statement.columnInt64(7));
  job.lastAttemptResult = parseOptionalJson(statement.columnTextNotNull(8));
}

}  // namespace

string Job::debugString() const {
  return cbl::concat("{id=", std::to_string(id), ", handler=", handler, ", key=\"", key,
                     "\", priority=", std::to_string(priority), ", parameters=", parameters.toJSON(), "}");
}

JobQueue::JobQueue(const string& databasePath) {
  m_database = Database::open(databasePath, {.synchronousMode = sqlite::SYNC_OFF}, [](Database& database) {
    database.execMany(R"(
        CREATE TABLE job(
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          handler TEXT NOT NULL,
          -- Typically a page name.
          key TEXT NOT NULL,
          -- The most important tasks have the smallest values.
          priority INT NOT NULL,
          -- Unix timestamp in seconds.
          inserted_on INT NOT NULL,
          -- Unix timestamp in seconds.
          run_after INT NOT NULL,
          -- Whether run_after >= now. Updated internally.
          can_be_processed INT,
          -- Arbitrary JSON.
          parameters TEXT,
          -- Unix timestamp in seconds.
          last_attempt INT,
          -- Arbitrary JSON.
          last_attempt_result TEXT
        );
        CREATE INDEX job_run_after_index ON job(run_after) WHERE can_be_processed = 0;
        CREATE INDEX job_priority_index ON job(priority, run_after, id) WHERE can_be_processed = 1;
        CREATE INDEX job_handler_key_index ON job(handler, key);
    )");
  });
}

JobId JobQueue::insertJobUnlocked(const Job& job, InsertMode insertMode) {
  if (job.handler.empty()) {
    throw std::invalid_argument("Invalid job: missing handler");
  }
  switch (insertMode) {
    case IGNORE_DUPS:
      break;
    case OVERWRITE_IF_EXISTS:
      m_database.exec("DELETE FROM job WHERE key = ?1 AND handler = ?2;", job.key, job.handler);
      break;
    case IGNORE_IF_EXISTS:
      Statement readJobs =
          m_database.prepareAndBind("SELECT id FROM job WHERE key = ?1 and handler = ?2;", job.key, job.handler);
      if (readJobs.step()) {
        return readJobs.columnInt64(0);
      }
      break;
  }
  int64_t runAfter = job.runAfter.isNull() ? 0 : job.runAfter.toTimeT();
  m_database.exec(
      "INSERT INTO job (handler, key, priority, inserted_on, run_after, can_be_processed, parameters) "
      "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);",
      job.handler, job.key, job.priority, Date::now().toTimeT(), runAfter, 0, job.parameters.toJSON());
  return m_database.lastInsertRowid();
}

JobId JobQueue::insertJob(const Job& job, InsertMode insertMode) {
  sqlite::WriteTransaction transaction(m_database, CBL_HERE);
  JobId newJobId = insertJobUnlocked(job, insertMode);
  transaction.commit();
  return newJobId;
}

void JobQueue::insertJobs(const vector<Job>& jobs, InsertMode insertMode) {
  if (jobs.empty()) {
    return;
  }
  sqlite::WriteTransaction transaction(m_database, CBL_HERE);
  for (const Job& job : jobs) {
    insertJobUnlocked(job, insertMode);
  }
  transaction.commit();
}

void JobQueue::updateStartFromMostRecent(JobId removedJobId) {
  Statement readJob = m_database.prepareAndBind("SELECT can_be_processed FROM job WHERE id = ?1;", removedJobId);
  if (!readJob.step() || readJob.columnInt(0) == 0) {
    return;
  }
  Statement readFirstJob =
      m_database.prepare("SELECT id FROM job WHERE can_be_processed = 1 ORDER BY priority, run_after, id;");
  bool removingOldestJob = readFirstJob.step() && readFirstJob.columnInt64(0) == removedJobId;
  m_database.saveGlobalInt64(START_MOST_MOST_RECENT, removingOldestJob ? 1 : 0);
}

void JobQueue::removeJob(JobId jobId) {
  removeJobs({jobId});
}

void JobQueue::removeJobs(const vector<JobId>& jobIds) {
  if (jobIds.empty()) {
    return;
  }
  sqlite::WriteTransaction transaction(m_database, CBL_HERE);
  if (jobIds.size() == 1) {
    updateStartFromMostRecent(jobIds[0]);
  }
  for (const JobId& jobId : jobIds) {
    m_database.exec("DELETE FROM job WHERE id = ?1;", jobId);
  }
  transaction.commit();
}

void JobQueue::rescheduleJobs(const vector<Rescheduling>& reschedulings) {
  if (reschedulings.empty()) {
    return;
  }
  sqlite::WriteTransaction transaction(m_database, CBL_HERE);
  int remaining = reschedulings.size();
  for (const Rescheduling& rescheduling : reschedulings) {
    remaining--;
    if (remaining == 0) {
      updateStartFromMostRecent(rescheduling.jobId);
    }
    m_database.exec(
        "UPDATE job SET can_be_processed = 0, run_after = ?1, last_attempt = ?2, last_attempt_result = ?3 "
        "WHERE id = ?4;",
        rescheduling.date.isNull() ? 0 : rescheduling.date.toTimeT(), Date::now().toTimeT(),
        rescheduling.result.toJSON(), rescheduling.jobId);
    if (rescheduling.priority.has_value()) {
      m_database.exec("UPDATE job SET priority = ?1 WHERE id = ?2;", *rescheduling.priority, rescheduling.jobId);
    }
  }
  transaction.commit();
}

void JobQueue::enumerateJobsToRun(const std::function<bool(Job& job)>& callback) {
  sqlite::WriteTransaction transaction(m_database, CBL_HERE);
  m_database.exec("UPDATE job SET can_be_processed = 1 WHERE run_after <= ?1 AND can_be_processed = 0",
                  Date::now().toTimeT());

  Statement readJobs =
      m_database.prepare(cbl::concat(BASE_JOB_SELECT, "WHERE can_be_processed = 1 ORDER BY priority, run_after, id;"));
  Statement readJobsBackward = m_database.prepare(
      cbl::concat(BASE_JOB_SELECT, "WHERE can_be_processed = 1 AND priority = ?1 ORDER BY run_after DESC, id DESC;"));
  bool startFromMostRecent = m_database.loadGlobalInt64(START_MOST_MOST_RECENT, 0) != 0;

  Job oldJob, newJob;
  optional<int> lastPriority;
  JobId previousNewJobId = INVALID_JOB_ID;
  bool middleReached = false;
  // Read by increasing priority. At a given priority, interleave the oldest and the newest job.
  while (readJobs.step()) {
    initJobFromStatement(oldJob, readJobs);
    if (lastPriority != oldJob.priority) {
      readJobsBackward.reset();
      readJobsBackward.bind(1, oldJob.priority);
      lastPriority = oldJob.priority;
      previousNewJobId = INVALID_JOB_ID;
      middleReached = false;
    }
    if (middleReached) {
      continue;
    }
    if (oldJob.id == previousNewJobId) {
      middleReached = true;
      continue;
    }
    CBL_ASSERT(readJobsBackward.step());
    initJobFromStatement(newJob, readJobsBackward);
    if (newJob.id == oldJob.id) {
      if (!callback(oldJob)) {
        break;
      }
      middleReached = true;
      continue;
    }
    Job& firstJob = startFromMostRecent ? newJob : oldJob;
    Job& secondJob = startFromMostRecent ? oldJob : newJob;
    if (!callback(firstJob) || !callback(secondJob)) {
      break;
    }
    previousNewJobId = newJob.id;
  }

  transaction.commit();
}

void JobQueue::enumerateJobsFromStatement(sqlite::Statement&& statement,
                                          const std::function<void(Job& job)>& callback) {
  sqlite::ReadTransaction transaction(m_database, CBL_HERE);
  Job job;
  while (statement.step()) {
    initJobFromStatement(job, statement);
    callback(job);
  }
}

void JobQueue::enumerateAllJobs(const std::function<void(Job& job)>& callback) {
  enumerateJobsFromStatement(m_database.prepare(cbl::concat(BASE_JOB_SELECT, ";")), callback);
}

void JobQueue::enumerateJobsByHandler(const string& handler, const std::function<void(Job& job)>& callback) {
  enumerateJobsFromStatement(m_database.prepareAndBind(cbl::concat(BASE_JOB_SELECT, "WHERE handler = ?1;"), handler),
                             callback);
}

vector<Job> JobQueue::getJobsByHandlerAndKey(const string& handler, const string& key) {
  vector<Job> jobs;
  enumerateJobsFromStatement(
      m_database.prepareAndBind(cbl::concat(BASE_JOB_SELECT, "WHERE handler = ?1 AND key = ?2;"), handler, key),
      [&](Job& job) { jobs.push_back(std::move(job)); });
  return jobs;
}

Date JobQueue::getFirstJobDate() {
  sqlite::ReadTransaction transaction(m_database, CBL_HERE);
  Date now = Date::now();
  Statement readRunnableJobs = m_database.prepare("SELECT 1 FROM job WHERE can_be_processed = 1 LIMIT 1;");
  if (readRunnableJobs.step()) {
    return now;
  }
  Statement readQueuedJobs =
      m_database.prepare("SELECT run_after FROM job WHERE can_be_processed = 0 ORDER BY run_after LIMIT 1;");
  if (readQueuedJobs.step()) {
    return std::max(now, parseOptionalDate(readQueuedJobs.columnInt64(0)));
  }
  return Date();
}

}  // namespace job_queue

#include "job_queue.h"
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/unittest.h"

using cbl::Date;
using cbl::DateDiff;
using std::make_unique;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

namespace job_queue {

class JobQueueTest : public cbl::Test {
private:
  void setUp() override {
    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T00:00:00Z"));
    jobQueue = make_unique<JobQueue>(":memory:");
  }
  vector<string> getQueuedJobsVec(bool sortResult) {
    vector<string> jobs;
    jobQueue->enumerateJobsToRun([&](Job& job) {
      jobs.push_back(job.key + (job.parameters.isNull() ? "" : ":" + job.parameters.toJSON()));
      return true;
    });
    if (sortResult) {
      std::sort(jobs.begin(), jobs.end());
    }
    return jobs;
  }
  string getQueuedJobs() { return cbl::join(getQueuedJobsVec(false), ","); }
  string getSortedQueuedJobs() { return cbl::join(getQueuedJobsVec(true), ","); }
  CBL_TEST_CASE(hideJobsInTheFuture) {
    vector<Job> jobs;
    jobs.push_back(Job{.handler = "test", .key = "A", .runAfter = Date::now() + DateDiff::fromSeconds(1)});
    jobs.push_back(Job{.handler = "test", .key = "B", .runAfter = Date::now() + DateDiff::fromSeconds(2)});
    jobs.push_back(Job{.handler = "test", .key = "C", .runAfter = Date::now() + DateDiff::fromSeconds(3)});
    jobQueue->insertJobs(jobs);
    CBL_ASSERT_EQ(getSortedQueuedJobs(), "");
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    CBL_ASSERT_EQ(getSortedQueuedJobs(), "A");
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    CBL_ASSERT_EQ(getSortedQueuedJobs(), "A,B");
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    CBL_ASSERT_EQ(getSortedQueuedJobs(), "A,B,C");
  }
  CBL_TEST_CASE(orderOfJobs) {
    jobQueue->insertJob(Job{.handler = "test", .key = "A", .priority = 0});
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    jobQueue->insertJob(Job{.handler = "test", .key = "B", .priority = 0});
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    jobQueue->insertJob(Job{.handler = "test", .key = "C", .priority = 1});
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    jobQueue->insertJob(Job{.handler = "test", .key = "D", .priority = 2});
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    jobQueue->insertJob(Job{.handler = "test", .key = "F", .priority = 1});
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    jobQueue->insertJob(Job{.handler = "test", .key = "G", .priority = 0});
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    jobQueue->insertJob(Job{.handler = "test", .key = "H", .priority = 1});
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    jobQueue->insertJob(Job{.handler = "test", .key = "I", .priority = 0});
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    jobQueue->insertJob(Job{.handler = "test", .key = "J", .priority = 0});
    CBL_ASSERT_EQ(getQueuedJobs(), "A,J,B,I,G,C,H,F,D");
  }
  CBL_TEST_CASE(removeJobs) {
    JobId jobA = jobQueue->insertJob(Job{.handler = "test", .key = "A"});
    JobId jobB = jobQueue->insertJob(Job{.handler = "test", .key = "B"});
    JobId jobC = jobQueue->insertJob(Job{.handler = "test", .key = "C"});
    JobId jobD = jobQueue->insertJob(Job{.handler = "test", .key = "D"});
    CBL_ASSERT_EQ(getQueuedJobs(), "A,D,B,C");
    jobQueue->removeJob(jobA);
    CBL_ASSERT_EQ(getQueuedJobs(), "D,B,C");  // Most recent job first because we just removed the oldest job.
    jobQueue->removeJob(jobC);
    CBL_ASSERT_EQ(getQueuedJobs(), "B,D");  // Oldest job first because we removed a job that was not the oldest.
    jobQueue->removeJob(jobB);
    CBL_ASSERT_EQ(getQueuedJobs(), "D");
    jobQueue->removeJob(jobD);
    CBL_ASSERT_EQ(getQueuedJobs(), "");
  }
  CBL_TEST_CASE(insertMode) {
    jobQueue->insertJob(Job{.handler = "test", .key = "B"}, IGNORE_IF_EXISTS);
    jobQueue->insertJob(Job{.handler = "test", .key = "A", .parameters = json::parse("101")});
    jobQueue->insertJob(Job{.handler = "test", .key = "A", .parameters = json::parse("102")});
    CBL_ASSERT_EQ(getSortedQueuedJobs(), "A:101,A:102,B");
    jobQueue->insertJob(Job{.handler = "test", .key = "A", .parameters = json::parse("103")}, IGNORE_IF_EXISTS);
    CBL_ASSERT_EQ(getSortedQueuedJobs(), "A:101,A:102,B");
    jobQueue->insertJob(Job{.handler = "test", .key = "A", .parameters = json::parse("104")}, OVERWRITE_IF_EXISTS);
    CBL_ASSERT_EQ(getSortedQueuedJobs(), "A:104,B");
    jobQueue->insertJob(Job{.handler = "test2", .key = "A", .parameters = json::parse("105")}, OVERWRITE_IF_EXISTS);
    CBL_ASSERT_EQ(getSortedQueuedJobs(), "A:104,A:105,B");
  }
  CBL_TEST_CASE(enumerateAllJobs) {
    jobQueue->insertJob(Job{.handler = "test1", .key = "A"});
    jobQueue->insertJob(Job{.handler = "test2", .key = "B", .runAfter = Date::now() + DateDiff::fromMinutes(1)});
    set<string> jobs;
    jobQueue->enumerateAllJobs([&](Job& job) { jobs.insert(cbl::concat(job.handler, "#", job.key)); });
    CBL_ASSERT_EQ(cbl::join(jobs, ","), "test1#A,test2#B");
  }
  CBL_TEST_CASE(enumerateJobsByHandler) {
    jobQueue->insertJob(Job{.handler = "test1", .key = "A"});
    jobQueue->insertJob(Job{.handler = "test2", .key = "B", .runAfter = Date::now() + DateDiff::fromMinutes(1)});
    jobQueue->insertJob(Job{.handler = "test1", .key = "C", .runAfter = Date::now() + DateDiff::fromMinutes(2)});
    set<string> jobs;
    jobQueue->enumerateJobsByHandler("test1", [&](Job& job) { jobs.insert(cbl::concat(job.handler, "#", job.key)); });
    CBL_ASSERT_EQ(cbl::join(jobs, ","), "test1#A,test1#C");
  }
  unique_ptr<JobQueue> jobQueue;
};

}  // namespace job_queue

int main() {
  job_queue::JobQueueTest().run();
  return 0;
}

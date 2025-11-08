#include "job_runner.h"
#include <cstring>
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "job_queue.h"

using cbl::Date;
using cbl::DateDiff;
using std::string;
using std::vector;

namespace job_queue {

class TestHandler : public JobHandler {
public:
  void startBatch(const vector<Job>& jobs, JobQueue& jobQueue) override {
    batchesAttempted++;
    for (const Job& job : jobs) {
      if (job.parameters.str() == "preparation_failure") {
        throw JobExecutionError(ErrorLevel::ERROR, "Test preparation failure", {});
      }
    }
    batchesProcessed++;
  };
  void run(const Job& job, JobQueue& jobQueue) override {
    const string& command = job.parameters.str();
    if (command == "failure") {
      throw JobExecutionError(ErrorLevel::ERROR, "Test failure", {});
    } else if (command.starts_with("generate:")) {
      generatedString += command.substr(strlen("generate:"));
    }
  }
  virtual int maxBatchSize() const { return 4; }

  string generatedString;
  int batchesAttempted = 0;
  int batchesProcessed = 0;
};

class RunJobsTest : public cbl::Test {
private:
  void setUp() override { Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T00:00:00Z")); }
  CBL_TEST_CASE(executeJobsInOrder) {
    JobQueue jobQueue(":memory:");
    jobQueue.insertJob({.handler = "testhandler",
                        .runAfter = Date::fromISO8601("2000-01-01T00:00:01Z"),
                        .parameters = json::Value("generate:a")});
    jobQueue.insertJob({.handler = "testhandler",
                        .runAfter = Date::fromISO8601("2000-01-01T00:00:02Z"),
                        .parameters = json::Value("generate:b")});
    jobQueue.insertJob({.handler = "testhandler",
                        .runAfter = Date::fromISO8601("2000-01-01T00:00:03Z"),
                        .parameters = json::Value("generate:c")});
    jobQueue.insertJob({.handler = "testhandler",
                        .runAfter = Date::fromISO8601("2000-01-01T00:00:04Z"),
                        .parameters = json::Value("generate:d")});

    TestHandler testHandler;
    JobHandlers jobHandlers = {{"testhandler", &testHandler}};
    runJobs(jobQueue, jobHandlers);
    CBL_ASSERT_EQ(testHandler.generatedString, "");
    CBL_ASSERT_EQ(testHandler.batchesProcessed, 0);
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    runJobs(jobQueue, jobHandlers);
    CBL_ASSERT_EQ(testHandler.generatedString, "a");
    CBL_ASSERT_EQ(testHandler.batchesProcessed, 1);
    Date::advanceFrozenClock(DateDiff::fromSeconds(2));
    runJobs(jobQueue, jobHandlers);
    CBL_ASSERT_EQ(testHandler.generatedString, "acb");
    CBL_ASSERT_EQ(testHandler.batchesProcessed, 2);
    Date::advanceFrozenClock(DateDiff::fromSeconds(1));
    runJobs(jobQueue, jobHandlers);
    CBL_ASSERT_EQ(testHandler.generatedString, "acbd");
    CBL_ASSERT_EQ(testHandler.batchesProcessed, 3);
  }
  CBL_TEST_CASE(splitBatch) {
    JobQueue jobQueue(":memory:");
    jobQueue.insertJob({.handler = "testhandler", .parameters = json::Value("generate:a")});
    jobQueue.insertJob({.handler = "testhandler", .parameters = json::Value("preparation_failure")});
    jobQueue.insertJob({.handler = "testhandler", .parameters = json::Value("generate:b")});
    jobQueue.insertJob({.handler = "testhandler", .parameters = json::Value("generate:c")});
    TestHandler testHandler;
    JobHandlers jobHandlers = {{"testhandler", &testHandler}};
    runJobs(jobQueue, jobHandlers);
    CBL_ASSERT_EQ(testHandler.generatedString, "acb");
    CBL_ASSERT_EQ(testHandler.batchesAttempted, 5);
    CBL_ASSERT_EQ(testHandler.batchesProcessed, 2);
  }
};

}  // namespace job_queue

int main() {
  job_queue::RunJobsTest().run();
  return 0;
}

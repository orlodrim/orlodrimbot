#ifndef ARTICLE_TO_DRAFT_MOVE_H
#define ARTICLE_TO_DRAFT_MOVE_H

#include "mwclient/wiki.h"
#include "orlodrimbot/wiki_job_runner/job_queue/job_queue.h"
#include "orlodrimbot/wiki_job_runner/job_queue/job_runner.h"

class ArticleToDraftMoveHandler : public job_queue::JobHandler {
public:
  explicit ArticleToDraftMoveHandler(mwc::Wiki* wiki) : m_wiki(wiki) {}
  void run(const job_queue::Job& job, job_queue::JobQueue& jobQueue, bool dryRun) override;

private:
  mwc::Wiki* m_wiki;
};

#endif

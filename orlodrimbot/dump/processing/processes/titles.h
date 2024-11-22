#ifndef TITLES_H
#define TITLES_H

#include <re2/re2.h>
#include <memory>
#include "process.h"

namespace dump_processing {

class Titles : public Process {
public:
  Titles() : Process({"input_disambigregexp", "output"}) {}
  void prepare() override;
  void processPage(Page& page) override;

private:
  std::unique_ptr<re2::RE2> m_reDisambiguation;
};

}  // namespace dump_processing

#endif

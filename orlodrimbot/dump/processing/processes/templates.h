#ifndef TEMPLATES_H
#define TEMPLATES_H

#include "process.h"

namespace dump_processing {

class Templates : public ProcessWithSingleOutputFile {
public:
  void processPage(Page& page) override;
};

}  // namespace dump_processing

#endif

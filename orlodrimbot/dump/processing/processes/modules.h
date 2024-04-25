#ifndef MODULES_H
#define MODULES_H

#include "process.h"

namespace dump_processing {

class Modules : public ProcessWithSingleOutputFile {
public:
  void processPage(Page& page) override;
};

}  // namespace dump_processing

#endif

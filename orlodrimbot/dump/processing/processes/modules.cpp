#include "modules.h"
#include "process.h"

namespace dump_processing {

void Modules::processPage(Page& page) {
  const int NS_MODULE = 828;
  if (page.namespace_() == NS_MODULE) {
    writePageToSimpleDump(page);
  }
}

}  // namespace dump_processing

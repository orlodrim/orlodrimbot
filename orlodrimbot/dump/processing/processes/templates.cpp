#include "templates.h"
#include "mwclient/wiki.h"
#include "process.h"

namespace dump_processing {

void Templates::processPage(Page& page) {
  if (page.namespace_() == mwc::NS_TEMPLATE) {
    writePageToSimpleDump(page);
  }
}

}  // namespace dump_processing

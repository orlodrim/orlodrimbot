#ifndef EMERGENCY_STOP_H
#define EMERGENCY_STOP_H

#include "cbl/date.h"
#include "mwclient/wiki.h"

class AdvancedUsersEmergencyStopTest {
public:
  AdvancedUsersEmergencyStopTest(mwc::Wiki& wiki);
  bool isEmergencyStopTriggered();

private:
  mwc::Wiki* m_wiki = nullptr;
  cbl::Date m_initializationDate;
};

#endif

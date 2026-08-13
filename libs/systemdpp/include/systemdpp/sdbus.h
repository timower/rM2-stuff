#pragma once

#include <unistdpp/unistdpp.h>

namespace systemdpp {

bool
waitForSleep();

bool
powerOff();

unistdpp::Result<unistdpp::FD>
getInhibitLock();

} // namespace systemdpp

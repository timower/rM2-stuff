#pragma once

#include <unistdpp/unistdpp.h>

namespace systemdpp {

bool
waitForSleep();

unistdpp::Result<unistdpp::FD>
getInhibitLock();

} // namespace systemdpp

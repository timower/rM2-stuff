#pragma once

namespace swtcon::vsync {

void
notifyVsyncThread();

void*
vsyncRoutine(void* arg);
} // namespace swtcon::vsync

#pragma once

#include "Addresses.h"

#include <pthread.h>

namespace swtcon::generator {

inline void
notifyGeneratorThread() {
  pthread_mutex_lock(generatorMutex);
  *generatorNotifyVar = 0;
  pthread_cond_broadcast(generatorCondVar);
  pthread_mutex_unlock(generatorMutex);
}

} // namespace swtcon::generator

#pragma once

#include "rm2fb/Message.h"

#include <unistdpp/unistdpp.h>

#include <mutex>
#include <vector>

// Client-side wrapper around the shared, lazily-connected control-socket
// fd - each method locks it for its own full request/reply exchange.
class ClientSocket {
public:
  ClientSocket();

  bool isValid() const { return sock.isValid(); }

  // Sends Init and receives the granted format + shared framebuffer.
  unistdpp::Result<void> init(bool ownSwtcon, FbFormat format, Buffer& fb);

  unistdpp::Result<void> sendIdleUpdate(bool idle);

  // Asks the server to mediate an open() of an evdev node - see
  // OpenInputDevice in Message.h.
  unistdpp::Result<int> openInputDevice(const char* pathname, int flags);

  bool sendUpdate(const UpdateParams& params);

  // Sends a whole batch in one message instead of one round-trip each -
  // see UpdateBatchHeader's comment in Message.h for why.
  bool sendUpdateBatch(const std::vector<UpdateParams>& updates);

private:
  std::lock_guard<std::mutex> lock;
  unistdpp::FD& sock;
};

// True for any "/dev/input/..." path short enough to fit
// OpenInputDevice::path - covers eventN nodes and by-id/by-path aliases.
bool
isInputDevicePath(const char* pathname);

// ClientSocket::openInputDevice(), adapted to open()'s error convention
// for direct use as an open()/open64() hook's return value.
int
openInputDeviceOrFail(const char* pathname, int flags);

// Free-function forwarders to ClientSocket - sendUpdate must stay a
// plain function pointer for installHooks() (Client.cpp's UpdateFn*).
bool
sendUpdate(const UpdateParams& params);

bool
sendUpdateBatch(const std::vector<UpdateParams>& updates);

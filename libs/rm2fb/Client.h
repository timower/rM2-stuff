#pragma once

#include "rm2fb/Message.h"

#include <unistdpp/unistdpp.h>

#include <vector>

// The client's persistent connection to the rm2fb control socket - lazily
// connected on first use, shared by doInit() and every update send.
unistdpp::FD&
getControlSocket();

bool
sendUpdate(const UpdateParams& params);

// Sends a whole batch in one message instead of one round-trip each - see
// UpdateBatchHeader's comment in Message.h for why.
bool
sendUpdateBatch(const std::vector<UpdateParams>& updates);

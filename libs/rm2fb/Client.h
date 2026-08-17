#pragma once

#include "rm2fb/Message.h"

#include <vector>

bool
sendUpdate(const UpdateParams& params);

// Sends a whole batch in one message instead of one round-trip each - see
// UpdateBatchHeader's comment in Message.h for why.
bool
sendUpdateBatch(const std::vector<UpdateParams>& updates);

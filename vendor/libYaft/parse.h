#pragma once

#ifdef __cplusplus

#include <cstdint>

extern "C" {
#endif

void
parse(struct terminal_t* term, const uint8_t* buf, int size);

#ifdef __cplusplus
}
#endif

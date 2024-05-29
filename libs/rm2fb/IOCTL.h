#pragma once

#include <cstdlib>

int
handleIOCTL(unsigned long request, char* ptr);

int
handleMsgSend(const void* buffer, size_t size);

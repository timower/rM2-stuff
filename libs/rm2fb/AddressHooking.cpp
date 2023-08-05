#include "Address.h"

#include "frida-gum.h"
#include <iostream>

bool
SimpleFunction::hook(void* replacement) {
  std::cerr << "Hooking " << address << " with " << (uintptr_t)replacement
            << "\n";

  auto* interceptor = gum_interceptor_obtain();

  auto result = gum_interceptor_replace(
    interceptor, (gpointer)address, (gpointer)replacement, nullptr);
  if (result != GUM_REPLACE_OK) {
    std::cerr << "Error replacing!\n";
    return false;
  }

  return true;
}

bool
InlinedFunction::hook(void* replacement) {
  std::cerr << "Hooking " << hookStart << " - " << hookEnd << " with "
            << (uintptr_t)replacement << "\n";

  const gsize distance = (intptr_t)hookEnd - (intptr_t)hookStart;

  gum_mprotect((gpointer)hookStart, distance, GUM_PAGE_RWX);

  GumArmWriter writer;
  gum_arm_writer_init(&writer, (gpointer)hookStart);

  gum_arm_writer_put_call_address_with_arguments(
    &writer, (GumAddress)replacement, 0);

  gum_arm_writer_put_branch_address(&writer, hookEnd);
  auto res = gum_arm_writer_flush(&writer);

  gum_mprotect((gpointer)hookStart, distance, GUM_PAGE_RX);

  return res;
}

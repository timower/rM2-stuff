#include "Server.h"
#include "Versions/Version.h"

#include <cstring>
#include <dlfcn.h>
#include <link.h>

namespace {
#define ALIGN(val, align) (((val) + (align) - 1) & ~((align) - 1))

struct BuildIdNote {
  ElfW(Nhdr) nhdr;

  std::array<char, 4> name;
  uint8_t build_id[0]; // NOLINT
};

BuildIdNote*
getBuildId(struct dl_phdr_info* info) {
  for (unsigned i = 0; i < info->dlpi_phnum; i++) {
    if (info->dlpi_phdr[i].p_type != PT_NOTE) {
      continue;
    }

    auto* note = (BuildIdNote*)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    ptrdiff_t len = info->dlpi_phdr[i].p_filesz;

    while (len >= ptrdiff_t(sizeof(BuildIdNote))) {
      if (note->nhdr.n_type == NT_GNU_BUILD_ID && note->nhdr.n_descsz != 0 &&
          note->nhdr.n_namesz == 4 &&
          memcmp(note->name.data(), "GNU", note->name.size()) == 0) {
        return note;
      }

      size_t offset = sizeof(ElfW(Nhdr)) + ALIGN(note->nhdr.n_namesz, 4) +
                      ALIGN(note->nhdr.n_descsz, 4);
      note = (BuildIdNote*)((char*)note + offset);
      len -= offset;
    }
  }
  return nullptr;
}

const AddressInfoBase*
getLibQscepaperAddrs() {
  void* handle = getQsgepaperHandle();
  if (handle == nullptr) {
    return nullptr;
  }

  BuildIdNote* buildIdPtr = nullptr;
  dl_iterate_phdr(
    [](struct dl_phdr_info* info, size_t size, void* buildIdPtr) {
      if (strcmp(info->dlpi_name,
                 "/usr/lib/plugins/scenegraph/libqsgepaper.so") != 0) {
        return 0;
      }

      *(BuildIdNote**)buildIdPtr = getBuildId(info);
      return 1;
    },
    &buildIdPtr);

  if (buildIdPtr == nullptr) {
    return nullptr;
  }

  BuildId id;
  std::memcpy(id.data(), buildIdPtr->build_id, id.size());
  return getAddresses(id);
}

const char*
getLibraryPath() {
  for (const char* path : {
         "/opt/lib/librm2fb_server.so",
         "/home/root/librm2fb_server.so",
         "/usr/lib/rm2fb_server.so",
       }) {
    if (access(path, R_OK) == 0) {
      return path;
    }
  }

  return nullptr;
}

} // namespace

int
main(int argc, char* argv[], char** envp) {
  if (const auto* addrs = getLibQscepaperAddrs(); addrs != nullptr) {
    return serverMain(argv[0], addrs);
  }
  std::cerr << "Could not find supported libqscepaper.so, starting xochitl "
               "with preload\n";

  const auto* libPath = getLibraryPath();
  if (libPath == nullptr) {
    std::cerr << "Could not find librm2fb_server.so!\n";
    return EXIT_FAILURE;
  }

  auto* preload = getenv("LD_PRELOAD");
  std::string preloadValue =
    std::string(libPath) +
    (preload == nullptr ? std::string() : ':' + std::string(preload));
  setenv("LD_PRELOAD", preloadValue.c_str(), 1);
  execvp("/usr/bin/xochitl", argv);
}

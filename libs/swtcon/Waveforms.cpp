#include "Waveforms.h"

#include "Addresses.h"
#include "Constants.h"

#include <iostream>
#include <string>
#include <vector>

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

namespace swtcon::waveform {

struct BootData {
  std::string deviceSerial;
  std::string serial2;
  std::string serial3;
  std::string epdSerial;
};

int
readBootData(BootData& out) {
  // Read the serial number from /dev/mmcblk2boot1
  auto* bootFile = fopen("/dev/mmcblk2boot1", "r");
  if (bootFile == nullptr) {
    perror("Error opening boot1 for reading");
    return -1;
  }
  char buf[32];

  for (int i = 0; i < 4; i++) {
    // 4 bytes length
    if (fread(buf, 4, 1, bootFile) != 1) {
      perror("Error reading length");
      fclose(bootFile);
      return -1;
    }
    int tmpBuf = *(int*)buf;
    int length = tmpBuf << 0x18 | (tmpBuf >> 8 & 0xff) << 0x10 |
                 (tmpBuf >> 0x10 & 0xff) << 8 | tmpBuf >> 0x18;

    // followed by length bytes of data
    if (fread(buf, length, 1, bootFile) != 1) {
      perror("Error reading data");
      fclose(bootFile);
      return -1;
    }

    // zero terminate
    buf[length] = '\0';
    switch (i) {
      case 0:
        out.deviceSerial = std::string(buf);
        break;
      case 1:
        out.serial2 = std::string(buf);
        break;
      case 2:
        out.serial3 = std::string(buf);
        break;
      case 3:
        out.epdSerial = std::string(buf);
        break;
      default:
        fclose(bootFile);
        return -1;
    }
  }

  fclose(bootFile);
  return 0;
}

int
decodeBarcode(std::string_view epdSerial) {
  if (epdSerial.length() != 25) {
    std::cerr << "Barcode length error\n";
    return -1;
  }

  int result = 0;

  // TODO: off by one? should be 7 and 8
  constexpr int offset = 7; // 6;

  int master = epdSerial[offset];

  if (((master - 'A') & 0xFF) < 8) {
    result = (master - 'A') * 10 + 100;
  } else if (((master - 'J') & 0xff) < 5) {
    result = (master - 'J') * 10 + 180;
  } else if (((master - 'Q') & 0xff) < 10) {
    result = (master - 'Q') * 10 + 230;
  } else if (master != '0') {
    return -1;
  }

  int second = epdSerial[offset + 1];
  if (((second - '0') & 0xff) >= 10) {
    return -1;
  }

  result = result + (second - '0');
  return result;
}

bool
hasEnding(const std::string& fullString, const std::string& ending) {
  if (fullString.length() >= ending.length()) {
    return (0 == fullString.compare(fullString.length() - ending.length(),
                                    ending.length(),
                                    ending));
  } else {
    return false;
  }
}

uint8_t*
readWbf(const char* path) {
  auto* file = fopen(path, "r");
  if (file == nullptr) {
    return nullptr;
  }

  fseek(file, 0, SEEK_END);
  auto size = ftell(file);

  if (size < 0x31) {
    std::cerr << "wbf file too small\n";
    fclose(file);
    return nullptr;
  }

  uint8_t* result = (uint8_t*)malloc(size);
  if (result == nullptr) {
    std::cerr << "wbf allocation error\n";
    fclose(file);
    return result;
  }

  fseek(file, 0, 0);
  if (fread(result, size, 1, file) != 1) {
    std::cerr << "wbf read error\n";
    free(result);
    fclose(file);
    return nullptr;
  }

  fclose(file);
  uint32_t fileSize = *(uint32_t*)(result + 4);
  if (fileSize != (uint32_t)size) {
    std::cerr << "file length mismatch\n";
    free(result);
    return nullptr;
  }

  return result;
}

std::string
findWaveformFile(int signature) {
  const std::string extension = ".wbf"; // TODO: ignore case
  const auto paths = { "/var/lib/uboot", "/usr/share/remarkable/" };

  std::vector<std::string> wbfPaths;

  for (const auto& path : paths) {
    DIR* dir = opendir(path);
    if (dir == nullptr) {
      continue;
    }

    dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
      if (entry->d_type != DT_REG) {
        continue;
      }

      auto fileName = std::string(entry->d_name);
      if (!hasEnding(fileName, extension)) {
        continue;
      }

      wbfPaths.push_back(path + fileName);
    }
    closedir(dir);
  }

  if (wbfPaths.empty()) {
    return "";
  }

  std::string fallback;
  for (const auto& path : wbfPaths) {
    auto* wbfData = readWbf(path.c_str());
    if (wbfData == nullptr) {
      std::cerr << "Error parsing wbf: " << path << std::endl;
      continue;
    }

    auto fpl_lot = *(uint16_t*)(wbfData + 0xe);
    free(wbfData); // TODO: unique ptr

    if (fpl_lot == signature) {
      return path;
    }

    fallback = path;
  }

  std::cout << "No matching waveform file found, using fallback: " << fallback
            << std::endl;
  return fallback;
}

uint8_t*
getTablePtr(uint8_t* waveformData, int mode, int temp) {
  if (mode > waveformData[0x25]) {
    std::cerr << "Error mode " << mode << " bigger than mode count "
              << (int)waveformData[0x25] << std::endl;
    return nullptr;
  }

  if (temp > waveformData[0x26]) {
    std::cerr << "Error temp " << temp << " bigger than temp count "
              << (int)waveformData[0x26] << std::endl;
    return nullptr;
  }

  uint32_t modeOffset =
    waveformData[0x20] | (waveformData[0x21] << 8) | (waveformData[0x22] << 16);
  uint32_t tempOffset =
    (*(uint32_t*)(waveformData + modeOffset + mode * 4)) & 0xffffff;
  uint32_t offset =
    (*(uint32_t*)(waveformData + tempOffset + temp * 4)) & 0xffffff;
  return waveformData + offset;
}

int
getTableAt(uint8_t* waveformData, int mode, int temp, uint8_t* out) {
  auto* tablePtr = getTablePtr(waveformData, mode, temp);
  if (tablePtr == nullptr) {
    return -1;
  }

  bool notAtRowEnd = true;

  // TODO: simplify this. Should just be a while and a for loop.
  int curSize = 0;
  int idx = 0;

  uint32_t elem = (uint32_t)*tablePtr;
  if ((uint32_t)waveformData[0x28] == elem) {
    return 0;
  }

  do {
    int nextIdx = idx + 1;
    auto* elemPtr = tablePtr + nextIdx;

    if (waveformData[0x29] == elem) {
      notAtRowEnd = !notAtRowEnd;
    } else {
      int length;
      if (notAtRowEnd) {
        length = (int)tablePtr[nextIdx] + 1;
        elemPtr = tablePtr + idx + 2;
        nextIdx = idx + 2;
      } else {
        length = 1;
      }

      int idx2 = 0;
      int lastSize = curSize;

      do {
        int nextIdx2 = idx2 + 1;
        curSize = lastSize + 4;
        if (out == nullptr) {
          idx2 = idx2 + 2;
          lastSize = lastSize + 8;
          if (nextIdx2 >= length) {
            break; // goto BreakInnerLoop;
          }
        } else {
          *out = (uint8_t)elem & 3;
          out[1] = (uint8_t)((elem << 0x1c) >> 0x1e);
          out[2] = (uint8_t)((elem << 0x1a) >> 0x1e);
          out[3] = (uint8_t)(elem >> 6);
          lastSize = curSize;
          idx2 = nextIdx2;
          out = out + 4;
        }
        curSize = lastSize;
      } while (idx2 < length);
    }
    // BreakInnerLoop:
    elem = (uint32_t)*elemPtr;

    idx = nextIdx;
  } while ((uint32_t)waveformData[0x28] != elem);

  return curSize;
}

int
readInitTable(uint8_t* waveformData) {
  uint32_t elementSize;
  if ((waveformData[0x24] & 0xc) == 4) {
    elementSize = 0x400;
  } else {
    elementSize = 0x100;
  }

  for (uint8_t tempIdx = 0; tempIdx <= waveformData[0x26]; tempIdx++) {
    auto size = getTableAt(waveformData, /* mode */ 0, tempIdx, nullptr);
    if (size < 1) {
      std::cerr << "Error reading init table (" << (int)tempIdx << ")\n";
      continue;
    }

    auto* tbl = (uint8_t*)malloc(size);
    auto size2 = getTableAt(waveformData, /* mode */ 0, tempIdx, tbl);
    if (size != size2) {
      std::cerr << "Error reading init table (" << (int)tempIdx << ")\n";
      free(tbl);
      continue;
    }

    auto elementCount = size / elementSize;
    auto* elements = (uint8_t*)malloc(elementCount);

    auto* tablePtr = &globalInitTable[tempIdx * 2];
    tablePtr[0] = elementCount;
    tablePtr[1] = (int)elements; // TODO: proper structure

    for (unsigned i = 0; i < elementCount; i++) {
      elements[i] = tbl[i * elementSize];
    }

    free(tbl);
  }

  return 0;
}

void
readWaveformTable(int idx,
                  uint8_t* waveformData,
                  int mode,
                  bool separatePartialTable,
                  bool skipInit) {
  uint32_t elementSize;
  uint32_t ptr1Inc;
  uint32_t ptr2Inc;

  if ((waveformData[0x24] & 0xc) == 4) {
    ptr1Inc = 2;
    ptr2Inc = 0x40;
    elementSize = 0x400;
  } else {
    ptr1Inc = 1;
    ptr2Inc = 0x10;
    elementSize = 0x100;
  }

  uint8_t tempCount = waveformData[0x26];
  auto* destPtr = globalTempTable + idx * 3;
  // Three values per table idx. Increment 26 = 2 temps + 3 * 8 idx values
  for (uint8_t tempIdx = 0; tempIdx <= tempCount; tempIdx++, destPtr += 26) {
    auto tableSize1 = getTableAt(waveformData, mode, tempIdx, nullptr);
    if (tableSize1 < 1) {
      std::cerr << "Error reading waveform table, skipping" << std::endl;
      continue;
    }

    auto* tablePtr = (uint8_t*)malloc(tableSize1);
    auto tableSize2 = getTableAt(waveformData, mode, tempIdx, tablePtr);
    if (tableSize1 != tableSize2) {
      std::cerr << "Error reading waveform table, skipping" << std::endl;
      continue;
    }

    auto elementCount = tableSize1 / elementSize;
    auto elementCountDiv8 = (elementCount + 7) >> 3; // TODO: why?
    auto totalSize = elementCountDiv8 * 0x200;

    auto* table1 = (uint8_t*)calloc(totalSize, 1);
    // 0 and 1 are temp range, 2, 3 and 4 are table size and two pointers.
    destPtr[2] = elementCount;
    destPtr[3] = (int)table1; // Full update table

    int iVar1;

    auto* tablePtr1 = tablePtr;
    for (int idx1 = 0; idx1 != 0x100; idx1 += 0x10, tablePtr1 += ptr1Inc) {
      auto* tablePtr2 = tablePtr1;
      for (int idx2 = 0; idx2 != 0x10; idx2 += 1, tablePtr2 += ptr2Inc) {

        // TODO: name these
        uint32_t uVar8 = 0;
        uint32_t uVar6 = 0;
        bool inInit = true;

        auto* elementPtr = tablePtr2;
        for (uint32_t elementIdx = 0; elementIdx != elementCount;
             elementIdx += 1, elementPtr += elementSize) {
          iVar1 = (int)uVar8 >> 3; // div by 8

          if (skipInit) {
            if ((*elementPtr & 3) != 0) {
              inInit = false;
            }

            if (inInit) {
              continue;
            }
          }

          // TODO: verify operator precedence
          uVar6 = (uVar6 | ((*elementPtr & 3) << ((uVar8 & 7) << 1))) & 0xffff;
          uVar8 += 1;

          if (((uVar8 & 7) != 0) && (elementCount - 1 != elementIdx)) {
            continue;
          }

          *(short*)(table1 + ((idx1 | idx2) + iVar1 * 0x100) * 2) =
            (short)uVar6;
          uVar6 = 0;
        }
      }
    }

    // Set partial table:
    if (!separatePartialTable) {
      destPtr[4] = (int)table1;
    } else {
      auto* table2 = (uint16_t*)malloc(totalSize);
      destPtr[4] = (int)table2;
      memcpy(table2, table1, totalSize);

      auto* table2End = table2 + elementCountDiv8 * 0x100;
      for (auto* table2Ptr = table2; table2Ptr != table2End;
           table2Ptr += 0x100) {
        for (auto* table2Ptr2 = table2Ptr; table2Ptr2 != table2Ptr + 0x110;
             table2Ptr2 += 0x11) {
          *table2Ptr2 = 0; // clear top 8 bits of short?
        }
      }
    }

    free(tablePtr);
  } // foreach tempIdx
}

int
initWaveforms() {
  BootData bootData;
  if (readBootData(bootData) != 0) {
    return -1;
  }
  std::cout << "Got epd serial: " << bootData.epdSerial << std::endl;

  auto signature = decodeBarcode(bootData.epdSerial);
  if (signature == -1) {
    std::cout << "Error decoding serial" << std::endl;
  }
  std::cout << "Got signature: " << signature << std::endl;

  auto waveformPath = findWaveformFile(signature);
  if (waveformPath.empty()) {
    std::cerr << "Error, no waveform files found\n";
    return -1;
  }
  std::cout << "Got wbf path: " << waveformPath << std::endl;

  auto* waveformData = readWbf(waveformPath.c_str());
  if (waveformData == nullptr) {
    return -1;
  }

  uint8_t* tempTable = waveformData + 0x30;
  int tempTableSize = *(waveformData + 0x26);

  for (int i = 0; i <= tempTableSize; i++) {
    auto* globalTablePtr = globalTempTable + i * 26;
    globalTablePtr[0] = tempTable[i];
    globalTablePtr[1] = i == tempTableSize ? 100 : tempTable[i + 1];

#ifndef NDEBUG
    std::cout << "temp range " << i << ": " << globalTablePtr[0] << " - "
              << globalTablePtr[1] << std::endl;
#endif
  }

  if (readInitTable(waveformData) != 0) {
    std::cerr << "Error reading waveform init table\n";
    free(waveformData);
    return -1;
  }

  readWaveformTable(0, waveformData, 1, /* sep. partial */ false, false); // DU
  readWaveformTable(1, waveformData, 2, /* sep. partial */ true, false); // GC16
  readWaveformTable(2, waveformData, 3, /* sep. partial */ true, false); // FAST
  readWaveformTable(3, waveformData, 6, /* sep. partial */ false, false); // GLF
  readWaveformTable(4, waveformData, 7, /* sep. partial */ false, false); // DU4

  readWaveformTable(5, waveformData, 1, /* sep. partial */ false, true); // DU
  readWaveformTable(6, waveformData, 2, /* sep. partial */ true, true);  // GC16
  readWaveformTable(7, waveformData, 7, /* sep. partial */ false, true); // DU4

  free(waveformData);
  return 0;
}

int
getTemperaturePath() {
  // TODO: fix this
  *tempPath = std::string("/sys/class/hwmon/hwmon0/temp0");
  return 0;
}

int
getTemperatureIdx(float temp) {
  for (int i = 0; i < 14; i++) {
    auto* globalTablePtr = globalTempTable + i * 26;
    if (temp < globalTablePtr[1]) {
      return i;
    }
  }

  std::cerr << "Temperature out of range?\n";

  return 13;
}

int
readTemperature(long* temperature) {
  if (!*haveTempPath) {
    if (getTemperaturePath() != 0) {
      std::cerr << "Error getting temp reader path\n";
      return -1;
    }
    *haveTempPath = true;
  }

  auto* file = fopen(tempPath->c_str(), "r");
  if (file == nullptr) {
    std::cerr << "Error reading temperature from: " << *tempPath << std::endl;
    *haveTempPath = false;
    return -1;
  }

  char buf[16] = { 0 };
  fread(buf, 1, 0xf, file);
  fclose(file);

  long val = strtol(buf, nullptr, 10);
  // TODO: error handling
  *temperature = val;
  return 0;
}

int
updateTemperature() {
  long temperature;
  int result = readTemperature(&temperature);
  if (result == 0) {
    *currentTempWaveform = getTemperatureIdx((float)temperature - 2);
    *currentTemperature = (float)temperature;
    std::cout << "Got current temperature: " << temperature
              << ", idx: " << *currentTempWaveform
              << " from path: " << *tempPath << std::endl;
  }

  timeval time;
  gettimeofday(&time, nullptr);
  *lastTempMeasureTime =
    (uint32_t)((double)time.tv_usec / 1000000.0 + (double)time.tv_sec);
  return result;
}

void
freeWaveforms() {
  // TODO: why hardcode temp range here?
  for (int tempIdx = 0; tempIdx < 14; tempIdx++) {
    auto* tempTablePtr = globalTempTable + tempIdx * 26;
    for (int i = 0; i < 8; i++) {
      auto* waveformPtr = tempTablePtr + i * 3;
      free((void*)waveformPtr[3]);
      if (waveformPtr[3] != waveformPtr[4]) {
        free((void*)waveformPtr[4]);
      }
    }
  }
}

InitWaveformInfo*
getInitWaveform(int tempIdx) {
  if (tempIdx > 13) {
    return nullptr;
  }
  return (InitWaveformInfo*)(globalInitTable + tempIdx * 2);
}

} // namespace swtcon::waveform

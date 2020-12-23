#include "interface.h"

#include "calc.h"
#include "exportvar.h"
#include "sendfile.h"
#include "var.h"

#define INVALID_MODEL -1

static LPCALC lpCalc;
char cache_dir[MAX_PATH];
static long staticThreadId = -1;

void
load_settings(LPCALC lpCalc, LPVOID lParam) {
  printf("RUNNING\n");
  lpCalc->running = TRUE;
}

void
calcinterface_init() {
  lpCalc = calc_slot_new();
  lpCalc->model = INVALID_MODEL;
  auto_turn_on = TRUE;
  calc_register_event(lpCalc, ROM_LOAD_EVENT, &load_settings, NULL);
}

bool
calcinterface_save_state(const char* path) {
  return false;
  // SAVESTATE_t* save = SaveSlot(lpCalc, "Wabbitemu", "Automatic save state");
  // bool wasSuccessful = false;
  // if (save != NULL) {
  //   wasSuccessful = WriteSave(path, save, ZLIB_CMP);
  //   FreeSave(save);
  // }

  // return save != NULL && wasSuccessful;
}

// int
// calcinterface_create_rom(JNIEnv* env,
//                          jclass classObj,
//                          jstring jOsPath,
//                          jstring jBootPath,
//                          jstring jRomPath,
//                          jint model) {
//   const char* osPath = (*env)->GetStringUTFChars(env, jOsPath, JNI_FALSE);
//   const char* bootPath = (*env)->GetStringUTFChars(env, jBootPath,
//   JNI_FALSE); const char* romPath = (*env)->GetStringUTFChars(env, jRomPath,
//   JNI_FALSE);
//
//   // Do not allow more than one calc currently
//   if (lpCalc) {
//     calc_slot_free(lpCalc);
//   }
//
//   lpCalc = calc_slot_new();
//   calc_init_model(lpCalc, model, NULL);
//
//   // slot stuff
//   strcpy(lpCalc->rom_path, romPath);
//   lpCalc->active = TRUE;
//   lpCalc->model = (CalcModel)model;
//   lpCalc->cpu.pio.model = model;
//   FILE* file = fopen(bootPath, "rb");
//   if (file == NULL) {
//     return -1;
//   }
//   writeboot(file, &lpCalc->mem_c, -1);
//   fclose(file);
//   remove(bootPath);
//   TIFILE_t* tifile = importvar(osPath, FALSE);
//   if (tifile == NULL) {
//     return -2;
//   }
//   int link_error = forceload_os(&lpCalc->cpu, tifile);
//   if (link_error != LERR_SUCCESS) {
//     return link_error;
//   }
//
//   calc_erase_certificate(lpCalc->mem_c.flash, lpCalc->mem_c.flash_size);
//   calc_reset(lpCalc);
//   // write the output from file
//   MFILE* romfile = ExportRom((char*)romPath, lpCalc);
//   if (romfile == NULL) {
//     return -3;
//   }
//
//   calc_register_event(lpCalc, ROM_LOAD_EVENT, &load_settings, NULL);
//   mclose(romfile);
//   return 0;
// }

int
calcinterface_load_file(const char* path) {
  TIFILE_t* tifile = importvar(path, TRUE);
  if (!tifile || !lpCalc) {
    return -1;
  }

  int result = SendFile(lpCalc, path, SEND_CUR);
  return result;
}

void
calcinterface_reset() {
  if (!lpCalc) {
    return;
  }

  lpCalc->fake_running = TRUE;
  calc_reset(lpCalc);
  lpCalc->fake_running = FALSE;
}

void
calcinterface_run() {
  calc_run_all();
}

void
calcinterface_pause() {
  if (!lpCalc) {
    return;
  }

  lpCalc->running = FALSE;
}

void
calcinterface_resume() {
  if (!lpCalc) {
    return;
  }

  lpCalc->running = TRUE;
}

int
calcinterface_getmodel() {
  if (!lpCalc) {
    return -1;
  }

  return lpCalc->model;
}

// JNIEXPORT jlong JNICALL
// Java_com_Revsoft_Wabbitemu_calc_CalcInterface_Tstates(JNIEnv* env,
//                                                       jclass classObj) {
//   if (!lpCalc) {
//     return -1;
//   }
//   return lpCalc->timer_c.tstates;
// }

void
calcinterface_set_speed(int speed) {
  lpCalc->speed = speed;
}

void
calcinterface_press_key(int group, int bit) {
  if (!lpCalc) {
    return;
  }

  keypad_press(&lpCalc->cpu, (int)group, (int)bit);
}

// JNIEXPORT void JNICALL
// Java_com_Revsoft_Wabbitemu_calc_CalcInterface_SetAutoTurnOn(JNIEnv* env,
//                                                             jclass classObj,
//                                                             jboolean turnOn)
//                                                             {
//   auto_turn_on = turnOn ? TRUE : FALSE;
// }

void
calcinterface_release_key(int group, int bit) {
  if (!lpCalc) {
    return;
  }

  keypad_release(&lpCalc->cpu, (int)group, (int)bit);
}

// void
// CopyGrayscale(int* screen, uint8_t* image) {
//   for (int i = 0, j = 0; i < LCD_HEIGHT * 128;) {
//     for (int k = 0; k < 96; i++, k++) {
//       uint8_t val = image[i];
//       screen[j++] = redPalette[val] + (greenPalette[val] << 8) +
//                     (bluePalette[val] << 16) + 0xFF000000;
//     }
//     i += 32;
//   }
// }

void*
calcinterface_get_lcd() {
  if (!lpCalc) {
    return FALSE;
  }

  LCDBase_t* lcd = lpCalc->cpu.pio.lcd;

  uint8_t* image;
  if (lcd != NULL && lcd->active) {
    image = lcd->image(lcd);
  } else {
    size_t size =
      (lpCalc->model == TI_84PCSE ? COLOR_LCD_DISPLAY_SIZE : GRAY_DISPLAY_SIZE);
    image = (uint8_t*)malloc(size);
    memset(image, 0, size);
    printf("lcd not active\n");
  }

  return image;
}

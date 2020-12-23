#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void
calcinterface_init();

bool
calcinterface_save_state(const char* path);
// int
// calcinterface_create_rom(JNIEnv* env,
//                          jclass classObj,
//                          jstring jOsPath,
//                          jstring jBootPath,
//                          jstring jRomPath,
//                          jint model) ;
int
calcinterface_load_file(const char* path);
void
calcinterface_reset();
void
calcinterface_run();
void
calcinterface_pause();
void
calcinterface_resume();
int
calcinterface_getmodel();

// JNIEXPORT jlong JNICALL
// Java_com_Revsoft_Wabbitemu_calc_CalcInterface_Tstates(JNIEnv* env,
//                                                       jclass classObj) ;
//
void
calcinterface_set_speed(int speed);
void
calcinterface_press_key(int group, int bit);

// JNIEXPORT void JNICALL
// Java_com_Revsoft_Wabbitemu_calc_CalcInterface_SetAutoTurnOn(JNIEnv* env,
//                                                             jclass classObj,
//                                                             jboolean turnOn)
//                                                             ;

void
calcinterface_release_key(int group, int bit);
void
CopyGrayscale(int* screen, uint8_t* image);
void*
calcinterface_get_lcd();
#ifdef __cplusplus
}
#endif

#pragma once

#include <stdint.h>

// APPLE compatiblity layer

struct input_event {
  timeval time;
  int16_t type;
  int16_t code;
  int32_t value;
};

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03

#define BTN_TOOL_PEN 0x140
#define BTN_TOUCH 0x14a

#define ABS_X 0x00
#define ABS_Y 0x01
#define ABS_DISTANCE 0x19
#define ABS_PRESSURE 0x18
#define ABS_DISTANCE 0x19

#define ABS_MT_SLOT 0x2f /* MT slot being modified */
#define ABS_MT_TOUCH_MAJOR 0x30 /* Major axis of touching ellipse */
#define ABS_MT_TOUCH_MINOR 0x31 /* Minor axis (omit if circular) */
#define ABS_MT_WIDTH_MAJOR 0x32 /* Major axis of approaching ellipse */
#define ABS_MT_WIDTH_MINOR 0x33 /* Minor axis (omit if circular) */
#define ABS_MT_ORIENTATION 0x34 /* Ellipse orientation */
#define ABS_MT_POSITION_X 0x35 /* Center X touch position */
#define ABS_MT_POSITION_Y 0x36 /* Center Y touch position */
#define ABS_MT_TOOL_TYPE 0x37 /* Type of touching device */
#define ABS_MT_BLOB_ID 0x38 /* Group a set of packets as a blob */
#define ABS_MT_TRACKING_ID 0x39 /* Unique ID of initiated contact */
#define ABS_MT_PRESSURE 0x3a /* Pressure on contact area */
#define ABS_MT_DISTANCE 0x3b /* Contact hover distance */
#define ABS_MT_TOOL_X 0x3c /* Center X tool position */
#define ABS_MT_TOOL_Y 0x3d /* Center Y tool position */

#define SYN_REPORT 0

/**
 * @ingroup events
 */
enum libevdev_read_flag {
  LIBEVDEV_READ_FLAG_SYNC = 1,   /**< Process data in sync mode */
  LIBEVDEV_READ_FLAG_NORMAL = 2, /**< Process data in normal mode */
  LIBEVDEV_READ_FLAG_FORCE_SYNC =
    4, /**< Pretend the next event is a SYN_DROPPED and
            require the caller to sync */
  LIBEVDEV_READ_FLAG_BLOCKING =
    8 /**< The fd is not in O_NONBLOCK and a read may block */
};

/**
 * @ingroup events
 */
enum libevdev_read_status {
  /**
   * libevdev_next_event() has finished without an error
   * and an event is available for processing.
   *
   * @see libevdev_next_event
   */
  LIBEVDEV_READ_STATUS_SUCCESS = 0,
  /**
   * Depending on the libevdev_next_event() read flag:
   * * libevdev received a SYN_DROPPED from the device, and the caller should
   * now resync the device, or,
   * * an event has been read in sync mode.
   *
   * @see libevdev_next_event
   */
  LIBEVDEV_READ_STATUS_SYNC = 1
};

struct udev_device;

const char*
udev_device_get_devnode(struct udev_device* udev_device);

const char*
udev_device_get_action(struct udev_device* udev_device);

struct libevdev*
libevdev_new(void);

void
libevdev_set_name(struct libevdev* dev, const char* name);

int
libevdev_enable_event_type(struct libevdev* dev, unsigned int type);
int
libevdev_enable_event_code(struct libevdev* dev,
                           unsigned int type,
                           unsigned int code,
                           const void* data);

enum libevdev_uinput_open_mode {
  /* intentionally -2 to avoid to avoid code like the below from accidentally
     working: fd = open("/dev/uinput", O_RDWR); // fails, fd is -1
          libevdev_uinput_create_from_device(dev, fd, &uidev); // may hide the
     error */
  LIBEVDEV_UINPUT_OPEN_MANAGED =
    -2 /**< let libevdev open and close @c /dev/uinput */
};

struct input_absinfo {
  int32_t value;
  int32_t minimum;
  int32_t maximum;
  int32_t fuzz;
  int32_t flat;
  int32_t resolution;
};

int
libevdev_uinput_create_from_device(const struct libevdev* dev,
                                   int uinput_fd,
                                   struct libevdev_uinput** uinput_dev);
void
libevdev_uinput_destroy(struct libevdev_uinput* uinput_dev);

int
libevdev_uinput_write_event(const struct libevdev_uinput* uinput_dev,
                            unsigned int type,
                            unsigned int code,
                            int value);

enum libevdev_grab_mode {
  LIBEVDEV_GRAB = 3,  /**< Grab the device if not currently grabbed */
  LIBEVDEV_UNGRAB = 4 /**< Ungrab the device if currently grabbed */
};

int
libevdev_grab(struct libevdev* dev, enum libevdev_grab_mode grab);

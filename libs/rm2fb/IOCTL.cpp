#include "IOCTL.h"

// rm2fb
#include "Client.h"
#include "SharedBuffer.h"

// libc
#include <cstring>
#include <iostream>
#include <linux/ioctl.h>

// 'linux'
#include <mxcfb.h>
#include <rm2.h>

namespace {

// NOLINTBEGIN
enum MSG_TYPE { INIT_t = 1, UPDATE_t, XO_t, WAIT_t };

struct xochitl_data {
  int x1;
  int y1;
  int x2;
  int y2;

  int waveform;
  int flags;
};

struct wait_sem_data {
  char sem_name[512];
};

struct swtfb_update {
  long mtype;
  struct {
    union {
      xochitl_data xochitl_update;

      struct mxcfb_update_data update;
      wait_sem_data wait_update;
    };

  } mdata;
};

// NOLINTEND

int
handleUpdate(const mxcfb_update_data& data) {
  const auto& rect = data.update_region;

  const int waveform = data.waveform_mode | UpdateParams::ioctl_waveform_flag;

  if (data.update_mode == RM2_UPDATE_MODE) {
    UpdateParams params;
    params.x1 = rect.left;
    params.y1 = rect.top;
    params.x2 = rect.left + rect.width - 1;
    params.y2 = rect.top + rect.height - 1;

    params.waveform = waveform;
    params.flags = data.flags;

    params.temperatureOverride = 0;
    params.extraMode = 0;

    return static_cast<int>(sendUpdate(params));
  }

  // There are three update modes on the rm2. But they are mapped to the five
  // rm1 modes as follows:
  //
  // 0: DU
  // 1: GC16: High fidelity / init with full refresh flag.
  // 2: GL16: Faster grayscale
  // 3: A2?: Pan & Zoom mode, 3.3+ GC16 mode

  // full = 1, partial = 0
  int flags = data.update_mode == UPDATE_MODE_FULL ? 0x1 : 0x0;

  // TODO: Get sync from client (wait for second ioctl? or look at stack?)
  // There are only two occasions when the original rm1 library sets sync to
  // true. Currently we detect them by the other data. Ideally we should
  // correctly handle the corresponding ioctl (empty rect and flags == 2?).
  if (data.waveform_mode == WAVEFORM_MODE_INIT &&
      data.update_mode == UPDATE_MODE_FULL) {
    flags |= 2;
    std::cerr << "SERVER: sync" << std::endl;
  } else if (rect.left == 0 && rect.top > 1800 &&
             (data.waveform_mode == WAVEFORM_MODE_GL16 ||
              data.waveform_mode == WAVEFORM_MODE_GC16) &&
             data.update_mode == UPDATE_MODE_FULL) {
    std::cerr << "server sync, x2: " << rect.width << " y2: " << rect.height
              << std::endl;
    flags |= 2;
  }

  if (data.waveform_mode == WAVEFORM_MODE_DU &&
      data.update_mode == UPDATE_MODE_PARTIAL) {
    // fast draw
    flags |= 4;
  }

  UpdateParams params;
  params.x1 = rect.left;
  params.y1 = rect.top;
  params.x2 = rect.left + rect.width - 1;
  params.y2 = rect.top + rect.height - 1;

  params.waveform = waveform;
  params.flags = flags;

  params.temperatureOverride = 0;
  params.extraMode = 0;

  sendUpdate(params);

  return 0;
}

} // namespace

int
handleIOCTL(unsigned long request, char* ptr) {
  if (request == MXCFB_SEND_UPDATE) {
    auto* update = (mxcfb_update_data*)ptr;
    return handleUpdate(*update);
  }

  if (request == MXCFB_SET_AUTO_UPDATE_MODE) {
    return 0;
  }

  if (request == MXCFB_WAIT_FOR_UPDATE_COMPLETE) {
    return 0;
  }

  if (request == FBIOGET_VSCREENINFO) {
    auto* screeninfo = (fb_var_screeninfo*)ptr;
    screeninfo->xres = fb_width;
    screeninfo->yres = fb_height;
    screeninfo->grayscale = 0;
    screeninfo->bits_per_pixel = 8 * fb_pixel_size;
    screeninfo->xres_virtual = fb_width;
    screeninfo->yres_virtual = fb_height;

    // set to RGB565
    screeninfo->red.offset = 11;
    screeninfo->red.length = 5;
    screeninfo->green.offset = 5;
    screeninfo->green.length = 6;
    screeninfo->blue.offset = 0;
    screeninfo->blue.length = 5;
    return 0;
  }

  if (request == FBIOPUT_VSCREENINFO) {
    return 0;
  }
  if (request == FBIOGET_FSCREENINFO) {

    auto* screeninfo = (fb_fix_screeninfo*)ptr;
    screeninfo->smem_len = fb_size;
    screeninfo->smem_start = (unsigned long)0x1000;
    screeninfo->line_length = fb_width * fb_pixel_size;
    constexpr char fb_id[] = "mxcfb";
    std::memcpy(screeninfo->id, fb_id, sizeof(fb_id));
    return 0;
  }

  std::cerr << "UNHANDLED IOCTL" << ' ' << request << std::endl;
  return 0;
}

int
handleMsgSend(const void* buffer, size_t size) {
  // NOLINTNEXTLINE
  const auto* update = reinterpret_cast<const swtfb_update*>(buffer);

  if (update->mtype != UPDATE_t) {
    std::cerr << "Unsupported msgsnd: " << update->mtype << "\n";
    return 0;
  }

  handleUpdate(update->mdata.update);

  return 0;
}

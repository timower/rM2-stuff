#pragma once

// Custom flag for ioctl to mark the update as a raw rm2-only update.
constexpr int RM2_UPDATE_MODE = 0x42;

// Taken from KOreader
static const int WAVEFORM_MODE_INIT = 0;
static const int WAVEFORM_MODE_DU = 1;
static const int WAVEFORM_MODE_GC16 = 2;
static const int WAVEFORM_MODE_GL16 = 3;
static const int WAVEFORM_MODE_A2 = 4;

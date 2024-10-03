reMarkable 2 - Framebuffer
==========================

SWTCON
------

The SWTCON is the software timing controller for the e-ink display in the
remarkable. It's used instead of a hardware one that's embedded in the iMX
processor. Its main job is to take in updates to the framebuffer and drive
the e-ink display with the correct voltages in the correct order to achieve
that change. These sequences are called waveforms, they depend on the
temperature, current state of the display and required speed.

The SWTCON is embedded inside of xochtil, which means we have to 'hook' into it
to be able to draw to the screen from other apps. There are a few functions of
interest in xochitl:
 * `createInstance`: Creates the QT EPDObject that owns the SWTCON state and the
   software framebuffer (stored inside a QImage).
 * `createThreads`: Called by `createInstance`, allocates the required buffers,
   locks and semaphores and then starts the generator and vsync threads.
 * `generatorThreadRoutine`: A thread that's responsible of taking in
   framebuffer updates and generating update frames that will get sent to the
   display.
 * `vsyncThreadRoutine`: Takes in generated update frames and 'pans' the display
   to show the with the correct timing.
 * `waitForStartup`: Function called by create instance to wait for the SWTCON
   threads to start.

Of course I just made up the names of these functions based on their behaviour.
The exact implementation of these functions differs between versions, and
some are inlined into each other in later (3.5+) xochitl versions.

<details>
<summary>
In pseudo code these functions look like:
</summary>
```cpp
EPDObject*
createInstance() {
  if (!QLockFile::tryLock("/tmp/epframebuffer.lock")) {
      LogError();
  }

  QImage screenBuffer;
  createThreads(screenBuffer.bits());

  wait_for_startup();

  puts("SWTCON initialized \\o/");

  return new EPDObject(screenBuffer);
}

void
wait_for_startup() {
  // clear the framebuffer, not in later xochitl versions.
  QImage::fill(param_1 + 0x20);

  // Post and wait until the global is flipped.
  initGlobal = 1;
  pthread_mutex_lock((pthread_mutex_t *)&DAT_008e4a6c);
  pthread_cond_broadcast((pthread_cond_t *)&DAT_008e4a88);
  pthread_mutex_unlock((pthread_mutex_t *)&DAT_008e4a6c);

  while (initGlobal == 1) {
    usleep(1000);
  }
}

void
createThreads(void* buffer) {
  allocateBuffers(buffer); // mallocs a backbuffer, memsets it, ...
  loadWaveforms();

  int fd = open("/dev/fb0");
  mmap(fd, ...);
  ioctl(fd); // Set var info, get fix info, ...

  updateTemperature(); // Used to select correct waveform

  pthread_mutex_init(&VsyncMutex1, NULL);
  pthread_mutex_init(&VsyncMutex2, NULL);
  pthread_cond_init(&VsyncCond1, NULL);

  pthread_create(&VsyncPThread, NULL, vsyncThreadRoutine, NULL);
  pthread_setname_np(VsyncPThread,"vsync-flip");
  setPriority(VsyncPThread,99);

  pthread_mutex_init(&GenMutex1,(pthread_mutexattr_t *)0x0);
  pthread_mutex_init(&GenMutex2,(pthread_mutexattr_t *)0x0);
  pthread_cond_init(&GenCond1,(pthread_condattr_t *)0x0);

  pthread_create(&GenPThread, NULL, generatorThreadRoutine, NULL);
  pthread_setname_np(GenPThread,"framegen");
  setPriority(GenPThread,0x62);
}

// Inlined into 'do_update'
void
updateGeneratorThread(void)

{
  pthread_mutex_lock((pthread_mutex_t *)&GenMutex2);
  updateGlobal = 0;
  pthread_cond_broadcast((pthread_cond_t *)&GenCond1);
  pthread_mutex_unlock((pthread_mutex_t *)&GenMutex2);
  return;
}


int
do_update(UpdateParams* params) {
  auto updateListElem = makeUpdate(params);
  // There's some processing of the update here, and the affected area
  // is copied to a separate buffer.

  bool syncFlagSet = params->updateFlags & 0x2;
  pthread_mutex_lock(&GenMutex1);
  // There's some of processing here. I suspect to combine consequtive
  // overlapping updates.
  std::list::push_back(GlobalList, updateListElem);
  pthread_mutex_unlock(&GenMutex1);

  updateGeneratorThread();

  if (syncFlagSet != 0) {
      while (!std::list::empty()) {
          usleep(1000);
      }
  }

  return 1;
}

void
EPDObject::update(EPDObject *this, QRect *area, int waveform, int flags)
{
  auto rect = this->image.rect();
  rect &= area;
  if ((rect.x0 <= rect.x1) && (rect.y0 <= rect.y1)) {
      UpdateParams params;
      switch(waveform) {
      case 0:
      case 3:
        params.waveform = 2;
        break;
      default:
        params.waveform = 0;
        break;
      case 2:
        params.waveform = 3;
        break;
      case 4:
        if (this->someFlag) {
          params.waveform = 0;
        } else {
          params.waveform = 8;
        }
        break;
      case 5:
        params.waveform = 1;
      }
      params.updateFlags = flags;
      do_update(&params);
  }
}
```
</details>


Waveform modes
--------------

The old ioctls recognized at least:
```
WAVEFORM_MODE_INIT = 0;
WAVEFORM_MODE_DU = 1;
WAVEFORM_MODE_GC16 = 2;
WAVEFORM_MODE_GL16 = 3;
WAVEFORM_MODE_A2 = 4;
```

These are mapped to internal waveform parameters passed to 'actual_update'.
In 2.15:
```cpp
switch(waveform) {
  case 0: // INIT
  case 3: // GL16
    newWaveform = 2;
    goto do_update;
  case 1: // DU
    newWaveform = 0;
    goto do_update;
  case 2: // GC16
  case 8: // ???
    newWaveform = 1;
    goto do_update;
  do_update:
    local_30 = local_3c;
    local_24 = local_38;
    local_2c = local_40;
    local_28 = local_34;
    local_20 = param_4;
    actual_update(&local_30);
    func_0x004d98a4(param_1,&local_40);
    return;
  case 4:
    break;
  case 5:
    break;
  case 6: // ??? Used for pan
    newWaveform = 3;
    goto do_update;
  case 7:
    break;
}
```

In 3.3:
```cpp
switch(param_3) {
  case 0: // INIT
  case 3: // GL16
    local_20.waveform = 2;
    break;

  case 1: // DU
  default:
    local_20.waveform = 0;
    break;

  case 2: // GC16
    local_20.waveform = 3;
    break;

  case 4: // A2
    if (*(char *)(param_1 + 0x3e) == '\0') {
      local_20.waveform = 0;
    }
    else {
      local_20.waveform = 8;
    }
    break;

  case 5: // ??? GC16
    local_20.waveform = 1;
}
```

In 3.5:
```cpp
switch(param_3) {
  case 0: // INIT
  case 3: // GL16
    local_20.waveform = 2;
    break;

  default: // DU
    local_20.waveform = 0;
    break;

  case 2: // GC16
    local_20.waveform = 3;
    break;

  case 4: // A2
    if (*(char *)(instance + 0x3e) == '\0') {
      local_20.waveform = 0;
    }
    else {
      local_20.waveform = 8;
    }
    break;

  case 5: // ??? GC16
    local_20.waveform = 1;
}
```

In 3.6+ they've changed the parameters of the update functions.
So there's not easy mapping, but update-dump tells us:
```
pen:          wave 1, flags 4, extra 1
highlighter:  wave 1, flags 4, extra 1

typing text:  wave 1, flags 4, extra 4
zoom/pan:     wave 6, flags 0, extra 4

b/w update:   wave 1, flags 0, extra 6
ui update:    wave 3, flags 0, extra 6

full refresh: wave 2, flags 1, extra 6
```

This means we probably have an internal waveform table that looks like:

| Description | 2.15 | 3.3 | 3.5 | 3.6+ |
| ----------- | ---- | --- | --- | ---- |
| DU (pen)    | 0    | 0   | 0   | 1    |
| GL16 (init) | 2    | 2   | 2   | 2    |
| GC16 (ui)   | 1    | 3   | 3   | 3    |
| A2 (pan)    | 3    | 8/0 | 8/0 | 6    |

reMarkable 2 - Framebuffer
==========================

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

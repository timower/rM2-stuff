# ReMarkable 2 Rendering Pipeline (`libqsgepaper.so`)

This document summarizes reverse engineering findings of the rendering pipeline inside the `libqsgepaper.so` Qt plugin (the proprietary reMarkable 2 rendering backend), specifically focusing on `swtcon` (the Software Timing Controller) and how `xochitl` translates UI updates into E-Ink waveforms.

## The `update_data` Structure
At the lowest level, `EPFramebufferSwtcon::update` queues a 32-byte `update_data` struct to the `swtcon` display thread. Key offsets in this struct dictate exactly how the screen region is drawn:
*   `0x00`: `x`
*   `0x04`: `y`
*   `0x08`: `width`
*   `0x0c`: `height`
*   `0x10`: `flags` (Update synchronization)
*   `0x14`: `update_mode` (E-Ink Waveform)
*   `0x18`: `zero`
*   `0x1C`: `pixel_mode` (Target buffer & processing pass)

---

## 1. The Dispatchers

### `EPFramebufferSwtcon::updateInBlocks` (The Spatial Optimizer)
This function acts as a spatial optimizer to minimize the overhead of sending highly fragmented `QRegion` shapes to the E-Ink controller.
1. It compares the total area of the `QRegion` rectangles against the region's overall bounding box.
2. If the region covers >24% of the bounding box (or has >64 rectangles), it aborts optimization and issues a single `update()` for the entire bounding box.
3. Otherwise, it loads the rectangles into a linked list, merges any intersecting or touching rectangles, and issues a separate `update()` call for each newly merged block.

### `EPFramebufferFusion::swapBuffers_impl` (The Waveform Dispatcher)
This is the master dispatcher called by Qt. It takes a "dirty" screen region and consults the `EPScreenModeMap` (which tracks what waveforms specific UI widgets requested) to decide how to draw each pixel.
*   **Synchronous Path (Full Refresh)**: If the `Sync` flag is passed, it ignores the mode map. It locks the queue, iterates over all dirty rectangles, and calls `update(..., 2, 6, 1)` and then `update(..., 2, 9, 1)`. This guarantees a full two-pass physical screen clear.
*   **Asynchronous Path**: It uses the `EPScreenModeMap` like a cookie cutter to slice up the dirty region. Parts requesting fast UI modes (Mode 0) are sent via `updateInBlocks` using `update_mode` 6. Parts requesting Direct Update (Mode 2) use `update_mode` 1. Any leftover regions fall through and are drawn using `update_mode` 3 (GL16).

---

## 2. Enums and Values

### `update_mode` (Waveforms)
Corresponds directly to E-Ink hardware waveforms:
*   `1`: **DU (Direct Update)** - Ultra-fast, monochrome-only update used for rapid pen strokes.
*   `2`: **GC16 (Grayscale 16)** - High Quality. Flashes the screen to clear ghosting. Used for full refreshes.
*   `3`: **GL16 (Grayscale Light 16)** - Medium Quality. Faster than GC16, doesn't flash, leaves some ghosting. Used for standard UI.
*   `6/7`: **A2 / DU4** - Fast-flashing partial update modes used for rapidly refreshing UI elements or quick scrolling (Panning, Shapes).

### `pixel_mode` (Buffers & Rendering Passes)
Because the reMarkable 2 uses a CPU-driven Software TCON (`swtcon`), `pixel_mode` instructs the TCON which memory buffer to read from and which physical processing pass to apply.
*   **`7` (The 8-bit Pen Buffer)**: The secret to the RM2's ultra-low latency pen. The digitizer thread draws raw pen strokes directly into a secondary 8-bit `backBuffer`. `pixel_mode = 7` tells the TCON to ignore the main Qt 16-bit canvas, read directly from the 8-bit buffer, and flash it to the screen instantly using the monochrome DU waveform.
*   **`6` vs `9` (The 16-bit Qt Canvas)**: Both instruct the TCON to read from the primary 16-bit Qt `data` buffer, but they represent different rendering phases:
    *   **`6` (Rough Pass)**: Used for Panning and Progress bars. Draws the pixels quickly, skipping complex waveform settling.
    *   **`9` (Settle Pass)**: Used for static UI elements. Applies a more precise, high-fidelity waveform calculation.
    *   *Note on Full Refreshes (corrected):* `swapBuffers_impl` does queue `6` then `9` on the same rectangles, for both the synchronous full refresh and the `UI` fallback branch below - but this does **not** produce two physical waveform phases. Both calls land inside a single `LockSwapMutex()`/`UnlockAndPostSwapMutex()` bracket, and `queue_update`'s own `subtract_update_region` (`libs/swtcon/update.cpp`) clips a newly-queued item's exact rect out of every not-yet-posted pending item before appending itself. Since both loops iterate the identical `QRegion`, the `9` call's subtraction pass finds the `6` call's entry for that same rect still sitting unposted in `accumList` and, being a byte-for-byte rect match, erases it outright (zero leftover pieces). So per rect, `6`'s CPU render (`dispatch_update_regions`) still runs and is thrown away, but only `9` (the Settle Pass) is ever posted to the display thread and reaches the panel - there is exactly one physical waveform phase here, not two. (Confirmed by direct reading of `update.cpp`'s `subtract_update_region`/`swtcon_update`, not just the decompile of `swapBuffers_impl` itself.)

### `flags` (Synchronization)
*   **`0`**: Async. Pushes the update to the queue without forcing the calling thread to wait.
*   **`1`**: Sync. Instructs the hardware queue to process the frame and ensures `WaitForUpdate()` blocks until completion.
*   **`2`**: FastDraw. Used for `swapBuffers_impl`'s entire async "mode 0 fast UI + pen" branch alike - `pixel_mode` (6 vs 7 for pen) is selected by a *different*, higher-level `EPFramebuffer::UpdateFlag` bit that never itself reaches `update_data`, not by this flag. Never set in the two-pass synchronous full-screen clear (that path only ever passes `flags=1`) - see `libs/swtcon/include/swtcon.h`'s `UpdateFlags` comment, which corrects this project's earlier "FullRefresh" mislabel of this same bit.

---

## 3. Mapping the Pipeline
The following mapping table (often seen in community tools like `rm2fb`) maps directly 1-to-1 with the decompiled logic inside `swapBuffers_impl`:

| Action | update_mode | flags | pixel_mode | Found in `swapBuffers_impl` |
| :--- | :--- | :--- | :--- | :--- |
| **Refresh** | 2 | 1 | 9 (`6` queued first but superseded - see note above) | The synchronous branch calls `update(..., 2, 6, 1)` followed by `update(..., 2, 9, 1)` on the same rects; only the `9` item survives to be posted. |
| **UI** | 3 | 0 | 9 (`6` queued first but superseded - see note above) | The default fallback branch for leftover regions calls `update(..., 3, 6, 0)` and `update(..., 3, 9, 0)` on the same rects; only the `9` item survives to be posted. |
| **Progress** | 1 | 0 | 6 | `updateInBlocks` for non-sync Mode 2 regions. |
| **Pen / Marker**| 1 | 2 | 7 | `updateInBlocks(..., 1, uVar4, 2)`. `uVar4` evaluates to `7` when `flags & 2` (`FastDraw`) is passed. |
| **Pan / Shape** | 6 | 0 | 6 | `updateInBlocks(..., 6, 6, 0)` for Mode 0 fast UI regions. |

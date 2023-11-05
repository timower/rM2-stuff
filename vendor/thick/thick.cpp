#include <math.h>

#include <Canvas.h>

using namespace rmlib;

/***********************************************************************
 *                                                                     *
 *                            X BASED LINES                            *
 *                                                                     *
 ***********************************************************************/

static void
x_perpendicular(Canvas& canvas,
                unsigned int color,
                int x0,
                int y0,
                int dx,
                int dy,
                int xstep,
                int ystep,
                int einit,
                int w_left,
                int w_right,
                int winit) {
  int x, y, threshold, E_diag, E_square;
  int tk;
  int error;
  int p, q;

  threshold = dx - 2 * dy;
  E_diag = -2 * dx;
  E_square = 2 * dy;
  p = q = 0;

  y = y0;
  x = x0;
  error = einit;
  tk = dx + dy - winit;

  while (tk <= w_left) {
    canvas.setPixel({ x, y }, color);
    if (error >= threshold) {
      x = x + xstep;
      error = error + E_diag;
      tk = tk + 2 * dy;
    }
    error = error + E_square;
    y = y + ystep;
    tk = tk + 2 * dx;
    q++;
  }

  y = y0;
  x = x0;
  error = -einit;
  tk = dx + dy + winit;

  while (tk <= w_right) {
    if (p) {
      canvas.setPixel({ x, y }, color);
    }
    if (error > threshold) {
      x = x - xstep;
      error = error + E_diag;
      tk = tk + 2 * dy;
    }
    error = error + E_square;
    y = y - ystep;
    tk = tk + 2 * dx;
    p++;
  }

  if (q == 0 && p < 2) {
    canvas.setPixel({ x, y }, color); // we need this for very thin lines
  }
}

static void
x_varthick_line(Canvas& canvas,
                unsigned int color,
                int x0,
                int y0,
                int dx,
                int dy,
                int xstep,
                int ystep,
                int thickness,
                int pxstep,
                int pystep) {
  int p_error, error, x, y, threshold, E_diag, E_square, length, p;
  int w_left, w_right;
  double D;

  p_error = 0;
  error = 0;
  y = y0;
  x = x0;
  threshold = dx - 2 * dy;
  E_diag = -2 * dx;
  E_square = 2 * dy;
  length = dx + 1;
  D = sqrt(dx * dx + dy * dy);

  for (p = 0; p < length; p++) {
    w_left = thickness * 2 * D;
    w_right = thickness * 2 * D;
    x_perpendicular(canvas,
                    color,
                    x,
                    y,
                    dx,
                    dy,
                    pxstep,
                    pystep,
                    p_error,
                    w_left,
                    w_right,
                    error);
    if (error >= threshold) {
      y = y + ystep;
      error = error + E_diag;
      if (p_error >= threshold) {
        x_perpendicular(canvas,
                        color,
                        x,
                        y,
                        dx,
                        dy,
                        pxstep,
                        pystep,
                        (p_error + E_diag + E_square),
                        w_left,
                        w_right,
                        error);
        p_error = p_error + E_diag;
      }
      p_error = p_error + E_square;
    }
    error = error + E_square;
    x = x + xstep;
  }
}

/***********************************************************************
 *                                                                     *
 *                            Y BASED LINES                            *
 *                                                                     *
 ***********************************************************************/

static void
y_perpendicular(Canvas& canvas,
                unsigned int color,
                int x0,
                int y0,
                int dx,
                int dy,
                int xstep,
                int ystep,
                int einit,
                int w_left,
                int w_right,
                int winit) {
  int x, y, threshold, E_diag, E_square;
  int tk;
  int error;
  int p, q;

  p = q = 0;
  threshold = dy - 2 * dx;
  E_diag = -2 * dy;
  E_square = 2 * dx;

  y = y0;
  x = x0;
  error = -einit;
  tk = dx + dy + winit;

  while (tk <= w_left) {
    canvas.setPixel({ x, y }, color);
    if (error > threshold) {
      y = y + ystep;
      error = error + E_diag;
      tk = tk + 2 * dx;
    }
    error = error + E_square;
    x = x + xstep;
    tk = tk + 2 * dy;
    q++;
  }

  y = y0;
  x = x0;
  error = einit;
  tk = dx + dy - winit;

  while (tk <= w_right) {
    if (p) {
      canvas.setPixel({ x, y }, color);
    }
    if (error >= threshold) {
      y = y - ystep;
      error = error + E_diag;
      tk = tk + 2 * dx;
    }
    error = error + E_square;
    x = x - xstep;
    tk = tk + 2 * dy;
    p++;
  }

  if (q == 0 && p < 2) {
    canvas.setPixel({ x, y }, color); // we need this for very thin lines
  }
}

static void
y_varthick_line(Canvas& canvas,
                unsigned int color,
                int x0,
                int y0,
                int dx,
                int dy,
                int xstep,
                int ystep,
                int thickness,
                int pxstep,
                int pystep) {
  int p_error, error, x, y, threshold, E_diag, E_square, length, p;
  int w_left, w_right;
  double D;

  p_error = 0;
  error = 0;
  y = y0;
  x = x0;
  threshold = dy - 2 * dx;
  E_diag = -2 * dy;
  E_square = 2 * dx;
  length = dy + 1;
  D = sqrt(dx * dx + dy * dy);

  for (p = 0; p < length; p++) {
    w_left = thickness * 2 * D;
    w_right = thickness * 2 * D;
    y_perpendicular(canvas,
                    color,
                    x,
                    y,
                    dx,
                    dy,
                    pxstep,
                    pystep,
                    p_error,
                    w_left,
                    w_right,
                    error);
    if (error >= threshold) {
      x = x + xstep;
      error = error + E_diag;
      if (p_error >= threshold) {
        y_perpendicular(canvas,
                        color,
                        x,
                        y,
                        dx,
                        dy,
                        pxstep,
                        pystep,
                        p_error + E_diag + E_square,
                        w_left,
                        w_right,
                        error);
        p_error = p_error + E_diag;
      }
      p_error = p_error + E_square;
    }
    error = error + E_square;
    y = y + ystep;
  }
}

/***********************************************************************
 *                                                                     *
 *                                ENTRY                                *
 *                                                                     *
 ***********************************************************************/

void
draw_thick_line(Canvas& canvas,
                unsigned int color,
                int x0,
                int y0,
                int x1,
                int y1,
                int width) {
  int dx, dy, xstep, ystep;
  int pxstep, pystep;

  dx = x1 - x0;
  dy = y1 - y0;
  xstep = ystep = 1;

  if (dx < 0) {
    dx = -dx;
    xstep = -1;
  }
  if (dy < 0) {
    dy = -dy;
    ystep = -1;
  }

  if (dx == 0)
    xstep = 0;
  if (dy == 0)
    ystep = 0;

  switch (xstep + ystep * 4) {
    case -1 + -1 * 4:
      pystep = -1;
      pxstep = 1;
      break; // -5
    case -1 + 0 * 4:
      pystep = -1;
      pxstep = 0;
      break; // -1
    case -1 + 1 * 4:
      pystep = 1;
      pxstep = 1;
      break; // 3
    case 0 + -1 * 4:
      pystep = 0;
      pxstep = -1;
      break; // -4
    case 0 + 0 * 4:
      pystep = 0;
      pxstep = 0;
      break; // 0
    case 0 + 1 * 4:
      pystep = 0;
      pxstep = 1;
      break; // 4
    case 1 + -1 * 4:
      pystep = -1;
      pxstep = -1;
      break; // -3
    case 1 + 0 * 4:
      pystep = -1;
      pxstep = 0;
      break; // 1
    case 1 + 1 * 4:
      pystep = 1;
      pxstep = -1;
      break; // 5
  }

  if (dx > dy) {
    x_varthick_line(
      canvas, color, x0, y0, dx, dy, xstep, ystep, width, pxstep, pystep);
  } else {
    y_varthick_line(
      canvas, color, x0, y0, dx, dy, xstep, ystep, width, pxstep, pystep);
  }
}

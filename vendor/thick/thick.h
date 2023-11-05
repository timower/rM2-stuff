#pragma once

namespace rmlib {
class Canvas;
}

void
draw_thick_line(rmlib::Canvas& canvas,
                unsigned int color,
                int x0,
                int y0,
                int x1,
                int y1,
                int width);

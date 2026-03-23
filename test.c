// ball.c
//
// Cleaner version: RL agent + cleaned graphics + oscillating target sprite

#include <stdlib.h>
#include "q_table.h"

// screen and tile constants
#define SCREEN_W   320
#define SCREEN_H   240
#define BLACK      0x0000
#define TILE       20
#define COLS       10
#define ROWS       10
#define NUM_MAPS   6
#define BALL_SIZE  8
#define SPEED      8

#define MAP_OFFSET_X  ((SCREEN_W - COLS*TILE) / 2)
#define MAP_OFFSET_Y  ((SCREEN_H - ROWS*TILE) / 2)

// colors
#define WHITE      0xFFFF
#define BLUE       0x001F
#define PINK       0xF81F

#define COL_BG         0x0000
#define COL_WALL_EDGE  0x39E7
#define COL_WALL_FACE  0xC638
#define COL_WALL_SHADE 0x7BEF
#define COL_WALL_TOP   0xEF7D

#define COL_BALL_OUT   0xF81F
#define COL_BALL_MID   0xFC1F
#define COL_BALL_IN    0xFFFF

#define COL_AGENT      0x001F
#define COL_AGENT_MID  0x4D7F
#define COL_AGENT_IN   0xFFFF

#define COL_TGT_MAIN   0xFFE0
#define COL_TGT_DIM    0xFD20
#define COL_TGT_CORE   0xFFFF

#define CAMERA_DISTANCE 135
#define HALF_BOX_WIDTH  16

short int Buffer1[240][512];

int target_col, target_row;
int agent_px, agent_py;

// target animation
int target_anim_tick = 0;
int target_anim_frame = 0;

// bobbing offsets
int target_bob_table[8] = {0, 1, 2, 1, 0, -1, -2, -1};

// 10x10 simple target sprite
// 0 = transparent, 1 = dim color, 2 = main color, 3 = white core
const unsigned char target_sprite_a[10][10] = {
    {0,0,0,1,1,1,1,0,0,0},
    {0,0,1,2,2,2,2,1,0,0},
    {0,1,2,2,2,2,2,2,1,0},
    {1,2,2,2,3,3,2,2,2,1},
    {1,2,2,3,3,3,3,2,2,1},
    {1,2,2,3,3,3,3,2,2,1},
    {1,2,2,2,3,3,2,2,2,1},
    {0,1,2,2,2,2,2,2,1,0},
    {0,0,1,2,2,2,2,1,0,0},
    {0,0,0,1,1,1,1,0,0,0}
};

const unsigned char target_sprite_b[10][10] = {
    {0,0,0,0,1,1,0,0,0,0},
    {0,0,0,1,2,2,1,0,0,0},
    {0,0,1,2,2,2,2,1,0,0},
    {0,1,2,2,3,3,2,2,1,0},
    {1,2,2,3,3,3,3,2,2,1},
    {1,2,2,3,3,3,3,2,2,1},
    {0,1,2,2,3,3,2,2,1,0},
    {0,0,1,2,2,2,2,1,0,0},
    {0,0,0,1,2,2,1,0,0,0},
    {0,0,0,0,1,1,0,0,0,0}
};

int maps[NUM_MAPS][ROWS][COLS] = {
{
    {1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,0,1,1,0,1},
    {1,0,1,0,0,0,0,1,0,1},
    {1,0,0,0,1,1,0,0,0,1},
    {1,0,0,0,1,1,0,0,0,1},
    {1,0,1,0,0,0,0,1,0,1},
    {1,0,1,1,0,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1}},
{
    {1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,1,0,0,0,1},
    {1,0,1,1,0,1,0,1,0,1},
    {1,0,1,0,0,0,0,1,0,1},
    {1,0,1,0,1,1,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,0,1,0,1,1,1},
    {1,0,0,0,0,1,0,0,0,1},
    {1,0,1,1,1,1,0,1,0,1},
    {1,1,1,1,1,1,1,1,1,1}},
{
    {1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,1,1,1,0,1},
    {1,0,1,0,0,0,0,0,0,1},
    {1,0,1,0,1,1,0,1,0,1},
    {1,0,1,0,1,1,0,1,0,1},
    {1,0,0,0,0,0,0,1,0,1},
    {1,0,1,1,1,1,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1}},
{
    {1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,1,0,0,0,0,1},
    {1,0,1,0,1,0,1,1,0,1},
    {1,0,1,0,0,0,0,1,0,1},
    {1,1,1,0,1,0,0,1,0,1},
    {1,0,0,0,1,0,1,1,0,1},
    {1,0,1,1,1,0,0,0,0,1},
    {1,0,0,0,0,0,1,1,0,1},
    {1,0,1,1,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1}},
{
    {1,1,1,1,1,1,1,1,1,1},
    {1,0,0,1,0,0,1,0,0,1},
    {1,0,0,0,0,0,1,0,0,1},
    {1,0,0,1,0,0,0,0,0,1},
    {1,1,0,1,1,1,1,0,1,1},
    {1,1,0,1,1,1,1,0,1,1},
    {1,0,0,0,0,0,1,0,0,1},
    {1,0,0,1,0,0,0,0,0,1},
    {1,0,0,1,0,0,1,0,0,1},
    {1,1,1,1,1,1,1,1,1,1}},
{
    {1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,0,1,0,0,1,0,0,1,1},
    {1,0,0,1,0,0,0,0,0,1},
    {1,0,0,1,0,0,0,1,0,1},
    {1,0,0,0,1,1,0,0,0,1},
    {1,0,1,0,0,0,0,0,0,1},
    {1,1,0,0,1,0,0,1,0,1},
    {1,0,0,0,1,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1}},
};

int px_to_col(int px) { return (px + BALL_SIZE/2 - MAP_OFFSET_X) / TILE; }
int py_to_row(int py) { return (py + BALL_SIZE/2 - MAP_OFFSET_Y) / TILE; }
int col_to_px(int col) { return MAP_OFFSET_X + col*TILE + (TILE-BALL_SIZE)/2; }
int row_to_py(int row) { return MAP_OFFSET_Y + row*TILE + (TILE-BALL_SIZE)/2; }

void plot_pixel(int x, int y, short int color) {
    if (x < 0 || x > 319 || y < 0 || y > 239) return;
    Buffer1[y][x] = color;
}

void draw_rect(int x, int y, int w, int h, short int color) {
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            plot_pixel(x + c, y + r, color);
}

void fill_circle(int cx, int cy, int r, short int color) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx*dx + dy*dy <= r*r)
                plot_pixel(cx + dx, cy + dy, color);
}

int abs(int x) { return x > 0 ? x : -x; }
void swap(int *x, int *y) { int t = *x; *x = *y; *y = t; }

void line(int x0, int y0, int x1, int y1, short color) {
    int steep = abs(y1-y0) > abs(x1-x0);
    if (steep) { swap(&x0, &y0); swap(&x1, &y1); }
    if (x0 > x1) { swap(&x0, &x1); swap(&y0, &y1); }

    int dx = x1 - x0;
    int dy = abs(y1 - y0);
    int err = -(dx / 2);
    int y = y0;
    int ys = (y0 < y1) ? 1 : -1;

    for (int i = x0; i <= x1; i++) {
        if (steep) plot_pixel(y, i, color);
        else       plot_pixel(i, y, color);

        err += dy;
        if (err > 0) {
            y += ys;
            err -= dx;
        }
    }
}

void quad(int x0, int y0, int x1, int y1,
          int x2, int y2, int x3, int y3, short color) {
    int xs[4] = {x0,x1,x2,x3};
    int ys[4] = {y0,y1,y2,y3};

    int ymin = ys[0], ymax = ys[0];
    for (int i = 1; i < 4; i++) {
        if (ys[i] < ymin) ymin = ys[i];
        if (ys[i] > ymax) ymax = ys[i];
    }

    for (int y = ymin; y <= ymax; y++) {
        int intersections[4];
        int count = 0;

        for (int i = 0; i < 4; i++) {
            int ax = xs[i], ay = ys[i];
            int bx = xs[(i+1)%4], by = ys[(i+1)%4];

            if ((y > ay && y <= by) || (y > by && y <= ay)) {
                intersections[count++] = ax + (y - ay) * (bx - ax) / (by - ay);
            }
        }

        for (int a = 0; a < count - 1; a++) {
            for (int b = a + 1; b < count; b++) {
                if (intersections[a] > intersections[b]) {
                    int t = intersections[a];
                    intersections[a] = intersections[b];
                    intersections[b] = t;
                }
            }
        }

        for (int a = 0; a < count - 1; a += 2)
            for (int x = intersections[a]; x <= intersections[a+1]; x++)
                plot_pixel(x, y, color);
    }
}

struct TwoDPoint { int x; int y; };
struct BoxPoints {
    struct TwoDPoint ftl,ftr,fbl,fbr,btl,btr,bbl,bbr;
};

struct TwoDPoint projectPoint(int x, int y, int z) {
    struct TwoDPoint p;
    int d = CAMERA_DISTANCE;
    p.x = (-(x) * d) / (z - d) + 160;
    p.y = (-(y) * d) / (z - d) + 120;
    return p;
}

struct BoxPoints getBoxPoints(int x, int y, int z) {
    struct BoxPoints b;
    int w = HALF_BOX_WIDTH;
    b.ftl = projectPoint(x-w, y+w, z+w);
    b.ftr = projectPoint(x+w, y+w, z+w);
    b.fbl = projectPoint(x-w, y-w, z+w);
    b.fbr = projectPoint(x+w, y-w, z+w);
    b.btl = projectPoint(x-w, y+w, z-w);
    b.btr = projectPoint(x+w, y+w, z-w);
    b.bbl = projectPoint(x-w, y-w, z-w);
    b.bbr = projectPoint(x+w, y-w, z-w);
    return b;
}

// cleaner wall box: just soft side shade + bright top
void drawBox(int x, int y, int z, short edge_color) {
    struct BoxPoints box = getBoxPoints(x, y, z);

    if (y < 0)
        quad(box.btl.x,box.btl.y, box.btr.x,box.btr.y,
             box.ftr.x,box.ftr.y, box.ftl.x,box.ftl.y, COL_WALL_SHADE);

    if (x > 0)
        quad(box.fbl.x,box.fbl.y, box.bbl.x,box.bbl.y,
             box.btl.x,box.btl.y, box.ftl.x,box.ftl.y, COL_WALL_FACE);

    if (x < 0)
        quad(box.ftr.x,box.ftr.y, box.btr.x,box.btr.y,
             box.bbr.x,box.bbr.y, box.fbr.x,box.fbr.y, COL_WALL_FACE);

    if (y > 0)
        quad(box.fbl.x,box.fbl.y, box.fbr.x,box.fbr.y,
             box.bbr.x,box.bbr.y, box.bbl.x,box.bbl.y, COL_WALL_SHADE);

    quad(box.ftl.x,box.ftl.y, box.ftr.x,box.ftr.y,
         box.fbr.x,box.fbr.y, box.fbl.x,box.fbl.y, COL_WALL_TOP);

    line(box.ftl.x,box.ftl.y, box.ftr.x,box.ftr.y, edge_color);
    line(box.ftr.x,box.ftr.y, box.fbr.x,box.fbr.y, edge_color);
    line(box.fbr.x,box.fbr.y, box.fbl.x,box.fbl.y, edge_color);
    line(box.fbl.x,box.fbl.y, box.ftl.x,box.ftl.y, edge_color);
}

void draw_wall_tile(int col, int row) {
    int x = col * 3 * TILE - 260;
    int y = row * 3 * TILE - 270;
    drawBox(x, y, -250, COL_WALL_EDGE);
}

void draw_ball(int x, int y, short int color) {
    int cx = x + BALL_SIZE/2;
    int cy = y + BALL_SIZE/2;

    if (color == BLACK) {
        draw_rect(x-2, y-2, BALL_SIZE+4, BALL_SIZE+4, BLACK);
        return;
    }

    if (color == COL_AGENT) {
        fill_circle(cx, cy, 4, COL_AGENT);
        fill_circle(cx, cy, 2, COL_AGENT_MID);
        fill_circle(cx, cy, 1, COL_AGENT_IN);
    } else {
        fill_circle(cx, cy, 4, COL_BALL_OUT);
        fill_circle(cx, cy, 2, COL_BALL_MID);
        fill_circle(cx, cy, 1, COL_BALL_IN);
    }

    plot_pixel(cx-1, cy-1, WHITE);
}

void draw_target_sprite_pixels(int cx, int cy, int frame) {
    const unsigned char (*sprite)[10] = (frame == 0) ? target_sprite_a : target_sprite_b;

    for (int r = 0; r < 10; r++) {
        for (int c = 0; c < 10; c++) {
            unsigned char v = sprite[r][c];
            short color;

            if (v == 0) continue;
            else if (v == 1) color = COL_TGT_DIM;
            else if (v == 2) color = COL_TGT_MAIN;
            else             color = COL_TGT_CORE;

            plot_pixel(cx - 5 + c, cy - 5 + r, color);
        }
    }
}

void draw_target(int col, int row, short int color) {
    int base_cx = MAP_OFFSET_X + col*TILE + TILE/2;
    int base_cy = MAP_OFFSET_Y + row*TILE + TILE/2;

    if (color == BLACK) {
        draw_rect(base_cx - 8, base_cy - 10, 16, 20, BLACK);
        return;
    }

    int bob = target_bob_table[target_anim_tick & 7];
    int cy = base_cy + bob;

    draw_target_sprite_pixels(base_cx, cy, target_anim_frame);
}

void draw_map(int m) {
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            Buffer1[y][x] = BLACK;

    for (int row = 0; row < ROWS; row++)
        for (int col = 0; col < COLS; col++)
            if (maps[m][row][col] == 1)
                draw_wall_tile(col, row);
}

void spawn_target(int m, int bc, int br) {
    int col, row;
    do {
        col = 1 + rand() % (COLS - 2);
        row = 1 + rand() % (ROWS - 2);
    } while (maps[m][row][col] != 0 || (col == bc && row == br));

    target_col = col;
    target_row = row;
    draw_target(target_col, target_row, COL_TGT_MAIN);
}

int hits_wall(int m, int px, int py) {
    int pts[4][2] = {
        {px,py}, {px+BALL_SIZE-1,py},
        {px,py+BALL_SIZE-1}, {px+BALL_SIZE-1,py+BALL_SIZE-1}
    };

    for (int i = 0; i < 4; i++) {
        int col = (pts[i][0] - MAP_OFFSET_X) / TILE;
        int row = (pts[i][1] - MAP_OFFSET_Y) / TILE;
        if (col < 0 || col >= COLS || row < 0 || row >= ROWS) return 1;
        if (maps[m][row][col] == 1) return 1;
    }
    return 0;
}

int reached_target(int px, int py) {
    int bc = (px + BALL_SIZE/2 - MAP_OFFSET_X) / TILE;
    int br = (py + BALL_SIZE/2 - MAP_OFFSET_Y) / TILE;
    return (bc == target_col && br == target_row);
}

void wait_for_vsync(void) {
    volatile int *ctrl = (int *)0xFF203020;
    *ctrl = 1;
    while ((*(ctrl+3) & 0x01) != 0);
}

volatile int *ps2 = (volatile int *)0xFF200100;
int ps2_read() {
    int val = *ps2;
    if (val & 0x8000) return val & 0xFF;
    return -1;
}

void reset_round(int *cm, int *px, int *py) {
    *cm = rand() % NUM_MAPS;
    draw_map(*cm);

    *px = col_to_px(1);
    *py = row_to_py(1);

    agent_px = col_to_px(2);
    agent_py = row_to_py(1);

    target_anim_tick = 0;
    target_anim_frame = 0;

    draw_ball(*px, *py, COL_BALL_OUT);
    draw_ball(agent_px, agent_py, COL_AGENT);
    spawn_target(*cm, 1, 1);
}

int main(void) {
    volatile int *pixel_ctrl = (int *)0xFF203020;
    *(pixel_ctrl+1) = (int)&Buffer1;
    *pixel_ctrl = 1;
    while ((*(pixel_ctrl+3) & 0x01) != 0);
    *(pixel_ctrl+1) = (int)&Buffer1;

    int cm = 0;
    draw_map(cm);

    int px = col_to_px(1);
    int py = row_to_py(1);
    draw_ball(px, py, COL_BALL_OUT);

    agent_px = col_to_px(2);
    agent_py = row_to_py(1);
    draw_ball(agent_px, agent_py, COL_AGENT);

    spawn_target(cm, 1, 1);

    int e0 = 0, skip = 0;
    int agent_tick = 0;
    int anim_tick = 0;

    while (1) {
        anim_tick++;
        if (anim_tick >= 20000) {
            anim_tick = 0;
            target_anim_tick++;
            target_anim_frame ^= 1;

            draw_target(target_col, target_row, BLACK);
            draw_target(target_col, target_row, COL_TGT_MAIN);
            draw_ball(agent_px, agent_py, COL_AGENT);
            draw_ball(px, py, COL_BALL_OUT);
        }

        agent_tick++;
        if (agent_tick >= 100000) {
            agent_tick = 0;

            int ac = px_to_col(agent_px);
            int ar = py_to_row(agent_py);

            agent_px = col_to_px(ac);
            agent_py = row_to_py(ar);

            int action = qt_best_action(ac, ar, target_col, target_row, cm);

            int nac = ac, nar = ar;
            if      (action == 0) nar -= 1;
            else if (action == 1) nar += 1;
            else if (action == 2) nac -= 1;
            else if (action == 3) nac += 1;

            if (nac >= 0 && nac < COLS && nar >= 0 && nar < ROWS
                && maps[cm][nar][nac] == 0) {
                draw_ball(agent_px, agent_py, BLACK);
                agent_px = col_to_px(nac);
                agent_py = row_to_py(nar);
                draw_target(target_col, target_row, COL_TGT_MAIN);
                draw_ball(agent_px, agent_py, COL_AGENT);
            }

            if (reached_target(agent_px, agent_py)) {
                reset_round(&cm, &px, &py);
                agent_tick = 0;
                continue;
            }
        }

        int b;
        while ((b = ps2_read()) >= 0) {
            if (skip) { skip = 0; e0 = 0; continue; }
            if (b == 0xF0) { skip = 1; continue; }
            if (b == 0xE0) { e0 = 1; continue; }

            int nx = px, ny = py;
            if (e0) {
                if      (b == 0x6B) nx -= SPEED;
                else if (b == 0x74) nx += SPEED;
                else if (b == 0x75) ny -= SPEED;
                else if (b == 0x72) ny += SPEED;
                e0 = 0;
            } else {
                if      (b == 0x1D) ny -= SPEED;
                else if (b == 0x1B) ny += SPEED;
                else if (b == 0x1C) nx -= SPEED;
                else if (b == 0x23) nx += SPEED;
            }

            if (!hits_wall(cm, nx, ny)) {
                draw_ball(px, py, BLACK);
                px = nx;
                py = ny;
                draw_target(target_col, target_row, COL_TGT_MAIN);
                draw_ball(agent_px, agent_py, COL_AGENT);
                draw_ball(px, py, COL_BALL_OUT);
            }

            if (reached_target(px, py)) {
                reset_round(&cm, &px, &py);
                agent_tick = 0;
            }
        }
    }

    return 0;
}
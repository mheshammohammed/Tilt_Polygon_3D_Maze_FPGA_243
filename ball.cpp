#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

// VGA pixel buffer base address (DE1-SoC)
#define VGA_BASE     0xFF200000
#define SCREEN_W     320
#define SCREEN_H     240

// Colors (16-bit RGB565)
#define BLACK        0x0000
#define WHITE        0xFFFF
#define RED          0xF800

#define BALL_SIZE    6
#define SPEED        2

volatile short *vga = (volatile short *)VGA_BASE;

// Write one pixel
void draw_pixel(int x, int y, short color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    *(vga + y * SCREEN_W + x) = color;
}

// Draw filled square (the ball)
void draw_ball(int x, int y, short color) {
    for (int dy = 0; dy < BALL_SIZE; dy++)
        for (int dx = 0; dx < BALL_SIZE; dx++)
            draw_pixel(x + dx, y + dy, color);
}

int main() {
    // Open keyboard event device
    // Check which event number yours is: ls /dev/input/
    int kbd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (kbd < 0) {
        printf("Could not open keyboard. Try event1 or event2.\n");
        return 1;
    }

    // Clear screen
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        vga[i] = BLACK;

    int px = SCREEN_W / 2;
    int py = SCREEN_H / 2;
    int dx = 0, dy = 0;

    // Draw initial ball
    draw_ball(px, py, RED);

    struct input_event ev;

    while (1) {
        dx = 0; dy = 0;

        // Read all pending key events
        while (read(kbd, &ev, sizeof(ev)) > 0) {
            if (ev.type == EV_KEY) {
                // key down (value 1) or held (value 2)
                if (ev.value == 1 || ev.value == 2) {
                    if (ev.code == KEY_LEFT)  dx = -SPEED;
                    if (ev.code == KEY_RIGHT) dx =  SPEED;
                    if (ev.code == KEY_UP)    dy = -SPEED;
                    if (ev.code == KEY_DOWN)  dy =  SPEED;
                }
                if (ev.value == 1 && ev.code == KEY_Q) goto done;
            }
        }

        if (dx != 0 || dy != 0) {
            // Erase only — redraw only if moved
            draw_ball(px, py, BLACK);

            px += dx;
            py += dy;

            // Clamp to screen
            if (px < 0) px = 0;
            if (px > SCREEN_W - BALL_SIZE) px = SCREEN_W - BALL_SIZE;
            if (py < 0) py = 0;
            if (py > SCREEN_H - BALL_SIZE) py = SCREEN_H - BALL_SIZE;

            draw_ball(px, py, RED);
        }

        usleep(16000); // ~60fps
    }

done:
    draw_ball(px, py, BLACK); // cleanup
    close(kbd);
    return 0;
}
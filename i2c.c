// accel_debug_ack.c
// PURPOSE: HARD DEBUG — checks ACK + WHO_AM_I

#include <stdlib.h>

#define SCREEN_W  320
#define SCREEN_H  240
#define BLACK     0x0000
#define GREEN     0x07E0
#define RED       0xF800
#define YELLOW    0xFFE0

short int Buffer1[240][512];

volatile int *jp1_data = (int *)0xFF200060;
volatile int *jp1_dir  = (int *)0xFF200064;

#define SCL_BIT  0
#define SDA_BIT  1

#define MPU_ADDR1 0x68
#define MPU_ADDR2 0x69
#define REG_WHO_AM_I 0x75

// ─── I2C ─────────────────────────────

void delay() { for (volatile int i = 0; i < 800; i++); }

void scl_high() { *jp1_dir &= ~(1 << SCL_BIT); delay(); }
void scl_low()  { *jp1_data &= ~(1 << SCL_BIT); *jp1_dir |= (1 << SCL_BIT); delay(); }

void sda_high() { *jp1_dir &= ~(1 << SDA_BIT); delay(); }
void sda_low()  { *jp1_data &= ~(1 << SDA_BIT); *jp1_dir |= (1 << SDA_BIT); delay(); }

int sda_read() {
    *jp1_dir &= ~(1 << SDA_BIT);
    delay();
    return (*jp1_data >> SDA_BIT) & 1;
}

void i2c_start() { sda_high(); scl_high(); sda_low(); scl_low(); }
void i2c_stop()  { sda_low(); scl_high(); sda_high(); }

int i2c_send(unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        if ((byte >> i) & 1) sda_high(); else sda_low();
        scl_high(); scl_low();
    }

    sda_high();
    scl_high();
    int nack = sda_read(); // 0 = ACK, 1 = FAIL
    scl_low();

    return nack;
}

unsigned char i2c_recv(int ack) {
    unsigned char val = 0;
    sda_high();

    for (int i = 7; i >= 0; i--) {
        scl_high();
        if (sda_read()) val |= (1 << i);
        scl_low();
    }

    if (ack) sda_low(); else sda_high();
    scl_high(); scl_low(); sda_high();

    return val;
}

// ─── TEST READ ───────────────────────

int test_device(unsigned char addr) {
    i2c_start();

    if (i2c_send(addr << 1)) {
        i2c_stop();
        return -1; // no ACK on address
    }

    if (i2c_send(REG_WHO_AM_I)) {
        i2c_stop();
        return -2; // no ACK on register
    }

    i2c_start();

    if (i2c_send((addr << 1) | 1)) {
        i2c_stop();
        return -3; // no ACK on read
    }

    unsigned char val = i2c_recv(0);
    i2c_stop();

    return val;
}

// ─── VGA ─────────────────────────────

void fill(short color) {
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            Buffer1[y][x] = color;
}

void wait_vsync() {
    volatile int *ctrl = (int *)0xFF203020;
    *ctrl = 1;
    while ((*(ctrl + 3) & 1));
}

// ─── MAIN ────────────────────────────

int main() {
    volatile int *pixel_ctrl = (int *)0xFF203020;

    *(pixel_ctrl + 1) = (int)Buffer1;
    *pixel_ctrl = 1;
    while ((*(pixel_ctrl + 3) & 1));
    *(pixel_ctrl + 1) = (int)Buffer1;

    // release lines
    *jp1_dir &= ~((1 << SCL_BIT) | (1 << SDA_BIT));

    // test both possible addresses
    int result1 = test_device(MPU_ADDR1);
    int result2 = test_device(MPU_ADDR2);

    while (1) {
        if (result1 == 0x68 || result2 == 0x68) {
            fill(GREEN);   // ✅ WORKING
        }
        else if (result1 >= 0 || result2 >= 0) {
            fill(YELLOW);  // ⚠️ responds but wrong value
        }
        else {
            fill(RED);     // ❌ NO DEVICE
        }

        wait_vsync();
    }
}
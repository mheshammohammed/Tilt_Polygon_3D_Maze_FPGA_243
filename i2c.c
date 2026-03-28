// accel_step1_raw.c
// FULL FILE — raw accelerometer test

#include <stdlib.h>

#define SCREEN_W  320
#define SCREEN_H  240
#define BLACK     0x0000
#define CYAN      0x07FF

short int Buffer1[240][512];

volatile int *jp1_data = (volatile int *)0xFF200060;
volatile int *jp1_dir  = (volatile int *)0xFF200064;

#define SCL_BIT  0
#define SDA_BIT  1

#define MPU_ADDR         0x68
#define REG_PWR_MGMT_1   0x6B
#define REG_ACCEL_XOUT_H 0x3B

// ─── I2C ─────────────────────────────

void i2c_delay() { for (volatile int i = 0; i < 500; i++); }

void scl_high() { *jp1_dir &= ~(1 << SCL_BIT); i2c_delay(); }
void scl_low()  { *jp1_data &= ~(1 << SCL_BIT); *jp1_dir |= (1 << SCL_BIT); i2c_delay(); }

void sda_high() { *jp1_dir &= ~(1 << SDA_BIT); i2c_delay(); }
void sda_low()  { *jp1_data &= ~(1 << SDA_BIT); *jp1_dir |= (1 << SDA_BIT); i2c_delay(); }

int sda_read() {
    *jp1_dir &= ~(1 << SDA_BIT);
    i2c_delay();
    return (*jp1_data >> SDA_BIT) & 1;
}

void i2c_start() { sda_high(); scl_high(); sda_low(); scl_low(); }
void i2c_stop()  { sda_low();  scl_high(); sda_high(); }

void i2c_send_byte(unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        if ((byte >> i) & 1) sda_high(); else sda_low();
        scl_high(); scl_low();
    }
    sda_high(); scl_high(); sda_read(); scl_low();
}

unsigned char i2c_recv_byte(int ack) {
    unsigned char byte = 0;
    sda_high();
    for (int i = 7; i >= 0; i--) {
        scl_high();
        if (sda_read()) byte |= (1 << i);
        scl_low();
    }
    if (ack) sda_low(); else sda_high();
    scl_high(); scl_low(); sda_high();
    return byte;
}

// ─── MPU ─────────────────────────────

void mpu_write_reg(unsigned char reg, unsigned char val) {
    i2c_start();
    i2c_send_byte(MPU_ADDR << 1);
    i2c_send_byte(reg);
    i2c_send_byte(val);
    i2c_stop();
}

void mpu_init() {
    *jp1_dir &= ~((1 << SCL_BIT) | (1 << SDA_BIT));
    mpu_write_reg(REG_PWR_MGMT_1, 0x00);
    for (volatile int i = 0; i < 5000000; i++);
}

void mpu_read_accel(short *ax, short *ay) {
    i2c_start();
    i2c_send_byte(MPU_ADDR << 1);
    i2c_send_byte(REG_ACCEL_XOUT_H);

    i2c_start();
    i2c_send_byte((MPU_ADDR << 1) | 1);

    unsigned char xh = i2c_recv_byte(1);
    unsigned char xl = i2c_recv_byte(1);
    unsigned char yh = i2c_recv_byte(1);
    unsigned char yl = i2c_recv_byte(0);

    i2c_stop();

    *ax = (short)((xh << 8) | xl);
    *ay = (short)((yh << 8) | yl);
}

// ─── VGA ─────────────────────────────

void plot_pixel(int x, int y, short c){
    if(x>=0 && x<320 && y>=0 && y<240)
        Buffer1[y][x]=c;
}

void fill_circle(int cx,int cy,int r,short c){
    for(int dy=-r;dy<=r;dy++)
        for(int dx=-r;dx<=r;dx++)
            if(dx*dx+dy*dy<=r*r)
                plot_pixel(cx+dx,cy+dy,c);
}

void clear_screen(){
    for(int y=0;y<240;y++)
        for(int x=0;x<320;x++)
            Buffer1[y][x]=BLACK;
}

void wait_for_vsync(){
    volatile int *ctrl=(int*)0xFF203020;
    *ctrl=1;
    while((*(ctrl+3)&1));
}

// ─── MAIN ────────────────────────────

int main(){
    volatile int *pixel_ctrl=(int*)0xFF203020;

    *(pixel_ctrl+1)=(int)Buffer1;
    *pixel_ctrl=1;
    while((*(pixel_ctrl+3)&1));
    *(pixel_ctrl+1)=(int)Buffer1;

    clear_screen();
    mpu_init();

    int old_x=160, old_y=120;

    while(1){
        short ax, ay;
        mpu_read_accel(&ax, &ay);

        // RAW movement
        int dot_x = 160 + ax / 128;
        int dot_y = 120 - ay / 128;

        fill_circle(old_x, old_y, 8, BLACK);
        fill_circle(dot_x, dot_y, 8, CYAN);

        old_x = dot_x;
        old_y = dot_y;

        wait_for_vsync();
    }

    return 0;
}
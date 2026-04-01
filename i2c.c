#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 240
short int Buffer1[240][512];

typedef struct {
    volatile unsigned int DATA;
    volatile unsigned int DDR;
} GPIO_t;

#define GPIO_BASE ((GPIO_t *)0xFF200060)
#define SCL_PIN 0
#define SDA_PIN 1

#define MPU_ADDR      0x68
#define REG_PWR_MGMT  0x6B
#define REG_WHO_AM_I  0x75
#define REG_ACCEL_X_H 0x3B

#define SENSITIVITY 500
#define DEADZONE    500

// ── Character buffer ──────────────────────────────────────────────────────────
#define CHAR_BASE 0x09000000

void write_char(int x, int y, char c) {
    volatile char *ptr = (volatile char *)(CHAR_BASE + x + 128 * y);
    *ptr = c;
}

void write_str(int x, int y, const char *s) {
    while (*s) write_char(x++, y, *s++);
}

void write_int(int x, int y, int val) {
    char buf[12];
    int i = 0, neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    if (val == 0) buf[i++] = '0';
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    if (neg) buf[i++] = '-';
    int start = x;
    for (int j = i - 1; j >= 0; j--) write_char(start++, y, buf[j]);
    for (int j = 0; j < 5; j++) write_char(start++, y, ' ');
}

void write_hex(int x, int y, unsigned int val) {
    const char *hex = "0123456789ABCDEF";
    write_char(x+0, y, '0');
    write_char(x+1, y, 'x');
    for (int i = 7; i >= 0; i--)
        write_char(x + 2 + (7-i), y, hex[(val >> (i*4)) & 0xF]);
}

void write_hex_byte(int x, int y, unsigned char val) {
    const char *hex = "0123456789ABCDEF";
    write_char(x,   y, hex[(val >> 4) & 0xF]);
    write_char(x+1, y, hex[val & 0xF]);
}

// ── Pixel helpers ─────────────────────────────────────────────────────────────
void plot_pixel(int x, int y, short color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    Buffer1[y][x] = color;
}

void dbg_color(short color) {
    for (int dy = 0; dy < 6; dy++)
        for (int dx = 0; dx < 6; dx++)
            plot_pixel(2+dx, 2+dy, color);
}

void wait_for_vsync(void) {
    volatile int *ctrl = (int *)0xFF203020;
    *ctrl = 1;
    while ((*(ctrl+3) & 0x01) != 0);
}

void clear_screen(void) {
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            Buffer1[y][x] = 0x0000;
}

void draw_dot(int x, int y, short color) {
    int r = 5;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx*dx + dy*dy <= r*r)
                plot_pixel(x+dx, y+dy, color);
}

// ── I2C ───────────────────────────────────────────────────────────────────────
void i2c_delay(void) { volatile int i; for (i = 0; i < 2000; i++); }

void sda_high(GPIO_t *g) { g->DDR  &= ~(1 << SDA_PIN); }
void sda_low (GPIO_t *g) { g->DATA &= ~(1 << SDA_PIN); g->DDR |= (1 << SDA_PIN); }
int  sda_read(GPIO_t *g) { g->DDR  &= ~(1 << SDA_PIN); i2c_delay(); return (g->DATA >> SDA_PIN) & 1; }

void scl_high(GPIO_t *g) { g->DDR  &= ~(1 << SCL_PIN); }
void scl_low (GPIO_t *g) { g->DATA &= ~(1 << SCL_PIN); g->DDR |= (1 << SCL_PIN); }

void i2c_start(GPIO_t *g) {
    sda_high(g); i2c_delay();
    scl_high(g); i2c_delay();
    sda_low(g);  i2c_delay();
    scl_low(g);  i2c_delay();
}

void i2c_stop(GPIO_t *g) {
    sda_low(g);  i2c_delay();
    scl_high(g); i2c_delay();
    sda_high(g); i2c_delay();
}

// returns 0=ACK, 1=NACK
int i2c_write_byte(GPIO_t *g, unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        if ((byte >> i) & 1) sda_high(g);
        else                 sda_low(g);
        i2c_delay();
        scl_high(g); i2c_delay();
        scl_low(g);  i2c_delay();
    }
    sda_high(g); i2c_delay();
    scl_high(g); i2c_delay();
    int nack = sda_read(g);
    scl_low(g);  i2c_delay();
    sda_high(g); i2c_delay();
    return nack;
}

unsigned char i2c_read_byte(GPIO_t *g, int send_ack) {
    unsigned char byte = 0;
    sda_high(g);
    for (int i = 7; i >= 0; i--) {
        i2c_delay();
        scl_high(g); i2c_delay();
        if (sda_read(g)) byte |= (1 << i);
        scl_low(g);
    }
    if (send_ack) sda_low(g);
    else          sda_high(g);
    i2c_delay();
    scl_high(g); i2c_delay();
    scl_low(g);  i2c_delay();
    sda_high(g); i2c_delay();
    return byte;
}

void mpu_write_reg(GPIO_t *g, unsigned char reg, unsigned char val) {
    i2c_start(g);
    i2c_write_byte(g, MPU_ADDR << 1);
    i2c_write_byte(g, reg);
    i2c_write_byte(g, val);
    i2c_stop(g);
}

// Returns number of NACKs encountered (0 = perfect)
int mpu_read_regs(GPIO_t *g, unsigned char reg, unsigned char *buf, int len) {
    int nacks = 0;
    i2c_start(g);
    nacks += i2c_write_byte(g, MPU_ADDR << 1);
    nacks += i2c_write_byte(g, reg);
    i2c_start(g);
    nacks += i2c_write_byte(g, (MPU_ADDR << 1) | 1);
    for (int i = 0; i < len; i++)
        buf[i] = i2c_read_byte(g, i < len - 1);
    i2c_stop(g);
    return nacks;
}

// Read a single register — key diagnostic
unsigned char mpu_read_reg(GPIO_t *g, unsigned char reg) {
    unsigned char val = 0;
    i2c_start(g);
    i2c_write_byte(g, MPU_ADDR << 1);
    i2c_write_byte(g, reg);
    i2c_start(g);
    i2c_write_byte(g, (MPU_ADDR << 1) | 1);
    val = i2c_read_byte(g, 0);  // NACK after last byte
    i2c_stop(g);
    return val;
}

// ── main ─────────────────────────────────────────────────────────────────────
int main(void) {
    volatile int *pixel_ctrl = (int *)0xFF203020;
    *(pixel_ctrl + 1) = (int)&Buffer1;
    *pixel_ctrl = 1;
    while ((*(pixel_ctrl + 3) & 0x01) != 0);
    *(pixel_ctrl + 1) = (int)&Buffer1;

    GPIO_t *gpio = GPIO_BASE;
    clear_screen();

    // Release both lines before anything
    gpio->DDR  &= ~((1 << SCL_PIN) | (1 << SDA_PIN));
    gpio->DATA &= ~((1 << SCL_PIN) | (1 << SDA_PIN));
    i2c_delay();

    // ── DIAGNOSTIC 1: WHO_AM_I — should return 0x68 ──────────────────────────
    write_str(0, 0, "WHO_AM_I:");
    unsigned char who = mpu_read_reg(gpio, REG_WHO_AM_I);
    write_hex_byte(10, 0, who);
    // 0x68 = pass, anything else = wiring/address problem
    if (who == 0x68) write_str(13, 0, "OK!");
    else             write_str(13, 0, "FAIL");

    // ── DIAGNOSTIC 2: Ping (write address only) ───────────────────────────────
    i2c_start(gpio);
    int ping_nack = i2c_write_byte(gpio, MPU_ADDR << 1);
    i2c_stop(gpio);
    write_str(0, 1, "PING:");
    write_str(6, 1, ping_nack ? "NACK(bad)" : "ACK(good)");

    // Wake up MPU
    mpu_write_reg(gpio, REG_PWR_MGMT, 0x00);
    i2c_delay(); i2c_delay(); i2c_delay();  // let it wake up

    // ── DIAGNOSTIC 3: Read PWR_MGMT — should be 0x00 after wake ─────────────
    unsigned char pwr = mpu_read_reg(gpio, REG_PWR_MGMT);
    write_str(0, 2, "PWR_MGMT:");
    write_hex_byte(10, 2, pwr);
    if (pwr == 0x00) write_str(13, 2, "OK!");
    else             write_str(13, 2, "FAIL");

    // ── DIAGNOSTIC 4: Raw bytes from accel registers ──────────────────────────
    unsigned char raw[6];
    int nacks = mpu_read_regs(gpio, REG_ACCEL_X_H, raw, 6);
    write_str(0, 3, "RAW BYTES:");
    for (int i = 0; i < 6; i++) {
        write_hex_byte(11 + i*3, 3, raw[i]);
        write_char(13 + i*3, 3, ' ');
    }
    write_str(0, 4, "NACKs:");
    write_int(7, 4, nacks);

    // Show DDR and DATA register state
    write_str(0, 5, "DDR:");
    write_hex(5, 5, gpio->DDR);
    write_str(0, 6, "DAT:");
    write_hex(5, 6, gpio->DATA);

    wait_for_vsync();

    // Blink debug square so we know we got here
    dbg_color(0x07FF);  // CYAN = finished init diagnostics

    int dot_x = SCREEN_W / 2;
    int dot_y = SCREEN_H / 2;
    int loop_count = 0;

    while (1) {
        unsigned char buf[6];
        int n = mpu_read_regs(gpio, REG_ACCEL_X_H, buf, 6);

        short ax = -(short)((buf[0] << 8) | buf[1]);
        short ay = -(short)((buf[2] << 8) | buf[3]);
        short az = -(short)((buf[4] << 8) | buf[5]);

        // Live values
        write_str(0, 7,  "ax:"); write_int(3, 7,  (int)ax);
        write_str(0, 8,  "ay:"); write_int(3, 8,  (int)ay);
        write_str(0, 9,  "az:"); write_int(3, 9,  (int)az);
        write_str(0, 10, "nk:"); write_int(3, 10, n);

        // Show raw bytes live too
        write_str(0, 11, "B:");
        for (int i = 0; i < 6; i++) {
            write_hex_byte(2 + i*3, 11, buf[i]);
            write_char(4 + i*3, 11, ' ');
        }

        // Loop counter so we can see if loop is running
        loop_count++;
        write_str(0, 12, "loop:"); write_int(5, 12, loop_count);

        draw_dot(dot_x, dot_y, 0x0000);

        if (ax >  DEADZONE) dot_x += (ax / SENSITIVITY) + 1;
        if (ax < -DEADZONE) dot_x -= ((-ax) / SENSITIVITY) + 1;
        if (az >  DEADZONE) dot_x += (az / SENSITIVITY) + 1;
        if (az < -DEADZONE) dot_x -= ((-az) / SENSITIVITY) + 1;
        if (ay >  DEADZONE) dot_y -= (ay / SENSITIVITY) + 1;
        if (ay < -DEADZONE) dot_y += ((-ay) / SENSITIVITY) + 1;
        if (az >  DEADZONE) dot_y -= (az / SENSITIVITY) + 1;
        if (az < -DEADZONE) dot_y += ((-az) / SENSITIVITY) + 1;

        if (dot_x < 5)           dot_x = 5;
        if (dot_x >= SCREEN_W-5) dot_x = SCREEN_W - 6;
        if (dot_y < 5)           dot_y = 5;
        if (dot_y >= SCREEN_H-5) dot_y = SCREEN_H - 6;

        draw_dot(dot_x, dot_y, 0xFFFF);

        // Toggle top-left square green/yellow each frame so we can see loop running
        dbg_color((loop_count & 1) ? 0x07E0 : 0xFFE0);

        wait_for_vsync();
    }

    return 0;
}
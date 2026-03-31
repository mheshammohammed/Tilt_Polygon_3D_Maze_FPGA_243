#include <stdint.h>

// ── Hardware ────────────────────────────────────────────────────────────────
typedef struct { volatile unsigned int DDR; volatile unsigned int DATA; } GPIO_t;

#define GPIO_BASE  ((GPIO_t *)0xFF200000)   // adjust to your Platform Designer addr
#define SCL_PIN    0
#define SDA_PIN    1

// ── MPU-6050 ─────────────────────────────────────────────────────────────────
#define MPU_ADDR      0x68   // AD0 low = 0x68, AD0 high = 0x69
#define REG_PWR_MGMT  0x6B
#define REG_ACCEL_X_H 0x3B   // X,Y,Z high/low follow sequentially

// ── I2C primitives ──────────────────────────────────────────────────────────

// Release a line high (open-drain: stop driving, let pull-up do the work)
void sda_high(GPIO_t *g){ pinMode(g, SDA_PIN, 0); }   // input = released
void sda_low (GPIO_t *g){ pinMode(g, SDA_PIN, 1); digitalWrite(g, SDA_PIN, 0); }
void scl_high(GPIO_t *g){ pinMode(g, SCL_PIN, 1); digitalWrite(g, SCL_PIN, 1); }
void scl_low (GPIO_t *g){ pinMode(g, SCL_PIN, 1); digitalWrite(g, SCL_PIN, 0); }
int  sda_read(GPIO_t *g){ pinMode(g, SDA_PIN, 0); return digitalRead(g, SDA_PIN); }

// START: SDA falls while SCL is high
void i2c_start(GPIO_t *g){
    sda_high(g); scl_high(g); delay(5);
    sda_low(g);              delay(5);
    scl_low(g);              delay(5);
}

// STOP: SDA rises while SCL is high
void i2c_stop(GPIO_t *g){
    sda_low(g);  scl_high(g); delay(5);
    sda_high(g);              delay(5);
}

// Send one byte, return 0 if ACK received from slave
int i2c_write_byte(GPIO_t *g, uint8_t byte){
    for(int i = 7; i >= 0; i--){
        if((byte >> i) & 1) sda_high(g);
        else                sda_low(g);
        delay(2);
        scl_high(g); delay(5);
        scl_low(g);  delay(2);
    }
    // Read ACK bit (slave pulls SDA low to ACK)
    sda_high(g);          // release SDA
    scl_high(g); delay(5);
    int nack = sda_read(g);
    scl_low(g);  delay(2);
    return nack;           // 0 = ACK ✓, 1 = NACK ✗
}

// Read one byte, send ACK if ack=1, NACK if ack=0 (last byte)
uint8_t i2c_read_byte(GPIO_t *g, int ack){
    uint8_t byte = 0;
    sda_high(g);  // release SDA for slave to drive
    for(int i = 7; i >= 0; i--){
        scl_high(g); delay(5);
        if(sda_read(g)) byte |= (1 << i);
        scl_low(g);  delay(5);
    }
    // Send ACK/NACK
    if(ack) sda_low(g);
    else    sda_high(g);
    scl_high(g); delay(5);
    scl_low(g);  delay(2);
    sda_high(g);
    return byte;
}

// ── MPU-6050 helpers ────────────────────────────────────────────────────────

void mpu_write_reg(GPIO_t *g, uint8_t reg, uint8_t val){
    i2c_start(g);
    i2c_write_byte(g, MPU_ADDR << 1);   // address + write bit
    i2c_write_byte(g, reg);
    i2c_write_byte(g, val);
    i2c_stop(g);
}

// Read `len` consecutive bytes starting at `reg` into `buf`
void mpu_read_regs(GPIO_t *g, uint8_t reg, uint8_t *buf, int len){
    i2c_start(g);
    i2c_write_byte(g, MPU_ADDR << 1);       // write phase: set register pointer
    i2c_write_byte(g, reg);
    i2c_start(g);                            // repeated START
    i2c_write_byte(g, (MPU_ADDR << 1) | 1); // read phase
    for(int i = 0; i < len; i++)
        buf[i] = i2c_read_byte(g, i < len-1); // ACK all but last
    i2c_stop(g);
}

void mpu_init(GPIO_t *g){
    mpu_write_reg(g, REG_PWR_MGMT, 0x00);  // wake up (clears sleep bit)
}

// Returns raw 16-bit accel values (±2g range by default)
typedef struct { int16_t x, y, z; } Accel;

Accel mpu_read_accel(GPIO_t *g){
    uint8_t buf[6];
    mpu_read_regs(g, REG_ACCEL_X_H, buf, 6);
    Accel a;
    a.x = (int16_t)((buf[0] << 8) | buf[1]);
    a.y = (int16_t)((buf[2] << 8) | buf[3]);
    a.z = (int16_t)((buf[4] << 8) | buf[5]);
    return a;
}

// ── Dot position from tilt ──────────────────────────────────────────────────
// VGA 640x480, dot starts centre.  Tune SENSITIVITY to taste.
#define VGA_W 640
#define VGA_H 480
#define SENSITIVITY 4000   // raw LSBs per full screen half-width

void accel_to_dot(Accel a, int *dot_x, int *dot_y){
    // X tilt  → horizontal,  Y tilt → vertical
    // Clamp to [0, VGA_W/H - 1]
    int x = (VGA_W/2) + (int)(a.x * (VGA_W/2)) / SENSITIVITY;
    int y = (VGA_H/2) - (int)(a.y * (VGA_H/2)) / SENSITIVITY; // flip Y axis
    if(x < 0) x = 0;  if(x >= VGA_W) x = VGA_W-1;
    if(y < 0) y = 0;  if(y >= VGA_H) y = VGA_H-1;
    *dot_x = x;
    *dot_y = y;
}

// ── Main loop ───────────────────────────────────────────────────────────────
int main(void){
    GPIO_t *gpio = GPIO_BASE;
    mpu_init(gpio);

    int dot_x = VGA_W/2, dot_y = VGA_H/2;

    while(1){
        Accel a = mpu_read_accel(gpio);
        accel_to_dot(a, &dot_x, &dot_y);
        vga_draw_dot(dot_x, dot_y);   // your existing VGA function
        delay(10000);                  // ~10 ms poll rate
    }
}
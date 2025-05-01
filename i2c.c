#include "i2c.h"
#include <stdio.h>
#include <string.h>

int i2c_init(void) {
    int fd = open(I2C_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open I2C device");
        return -1;
    }
    return fd;
}

void i2c_close(int fd) {
    close(fd);
}

void i2c_write_byte(int fd, uint8_t saddr, uint8_t maddr, uint8_t data) {
    if (ioctl(fd, I2C_SLAVE, saddr) < 0) {
        perror("Failed to set slave address");
        return;
    }
    
    uint8_t buf[2] = {maddr, data};
    if (write(fd, buf, 2) != 2) {
        perror("Failed to write byte to I2C");
    }
}

void i2c_write_multi(int fd, uint8_t saddr, uint8_t maddr, uint8_t *buffer, uint16_t length) {
    if (ioctl(fd, I2C_SLAVE, saddr) < 0) {
        perror("Failed to set slave address");
        return;
    }
    
    uint8_t buf[length + 1];
    buf[0] = maddr;
    memcpy(&buf[1], buffer, length);
    
    if (write(fd, buf, length + 1) != (length + 1)) {
        perror("Failed to write multiple bytes to I2C");
    }
}
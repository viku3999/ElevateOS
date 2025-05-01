#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define I2C_DEVICE "/dev/i2c-1"

int i2c_init(void);
void i2c_close(int fd);
void i2c_write_byte(int fd, uint8_t saddr, uint8_t maddr, uint8_t data);
void i2c_write_multi(int fd, uint8_t saddr, uint8_t maddr, uint8_t *buffer, uint16_t length);

#endif
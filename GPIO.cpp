#include <iostream>
#include <cstdint>
#include <cstdio>
#include <semaphore.h>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>
#include <sched.h>

#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
 
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "Sequencer.hpp"
#include <atomic>
#include <syslog.h>

#include <fstream>
#include <linux/gpio.h>
 
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include "GPIO.hpp"
#include <fstream>

#define DEV_NAME "/dev/gpiochip0"

#define BCM2835_PERI_BASE 0xFE000000   // For Raspberry Pi 4
#define GPIO_BASE (BCM2835_PERI_BASE + 0x200000) // GPIO controller base
#define BLOCK_SIZE (4*1024)

// Pointer to the mapped memory
volatile unsigned int *gpio;

// Function to set up the memory mapping for GPIO access
void setup_gpio() {
    int mem_fd;
    void *gpio_map;

    if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        perror("Failed to open /dev/mem, try running as root");
        exit(EXIT_FAILURE);
    }

    // Map GPIO memory to our address space
    gpio_map = mmap(
        NULL,                 // Any address in our space will do
        BLOCK_SIZE,           // Map length
        PROT_READ | PROT_WRITE, // Enable reading & writing to mapped memory
        MAP_SHARED,           // Shared with other processes
        mem_fd,               // File descriptor for /dev/mem
        GPIO_BASE             // Offset to GPIO peripheral
    );

    close(mem_fd);

    if (gpio_map == MAP_FAILED) {
        perror("mmap error");
        exit(EXIT_FAILURE);
    }

    // volatile pointer to prevent compiler optimizations
    gpio = (volatile unsigned int *)gpio_map;
}

// Set GPIO pin as output
void set_gpio_output(int pin) {
    uint32_t reg_num = pin / 10;
    uint32_t bit_num = (pin % 10) * 3;
    uint32_t mask = 0b111 << bit_num;
    
    volatile uint32_t* gpfsel = gpio + reg_num;
    *gpfsel = (*gpfsel & ~mask) | (0b001 << bit_num);
}

// Set GPIO pin as input
void set_gpio_input(int pin) {
    uint32_t reg_num = pin / 10;
    uint32_t bit_num = (pin % 10) * 3;
    uint32_t mask = 0b111 << bit_num;
    
    volatile uint32_t* gpfsel = gpio + reg_num;
    *gpfsel = (*gpfsel & ~mask) | (0b000 << bit_num);
}

// Set GPIO function
void set_gpio(int pin) {
    gpio[7] = (1 << pin);
}

// Reset GPIO function
void reset_gpio(int pin) {
    gpio[10] = (1 << pin);
}

int GPIO_Init(int GPIO_Pin, int Direction){
    static int init = 0;
    if(!init){
        setup_gpio();
        init = 1;    
    }

    if(Direction == GPIO_OUT){
        set_gpio_output(GPIO_Pin);
    }
    else if(Direction == GPIO_IN){
        set_gpio_input(GPIO_Pin);
    }
    else{
        std::cerr << "Invalid direction" << std::endl;
        return 0;
    }
    return 1;
}

int GPIO_Set_Value(int GPIO_Pin, int Value){
    if(Value == GPIO_HIGH){
        set_gpio(GPIO_Pin);
    }
    else if (Value == GPIO_LOW){
        reset_gpio(GPIO_Pin);
    }
    else{
        std::cerr << "Invalid value" << std::endl;
        return 0;
    }
    return 1;
}

// Get GPIO value
int GPIO_Get_Value(int GPIO_Pin, int *Value){
    uint32_t reg_num = GPIO_Pin / 32;
    uint32_t bit_num = GPIO_Pin % 32;
    
    volatile uint32_t* gplev = gpio + (13 + reg_num);
    *Value = (*gplev >> bit_num) & 0x1;
    return 1;
}
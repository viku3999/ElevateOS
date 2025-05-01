#include "keypad.h"
#include <stdio.h>

// BCM GPIO pins
const uint8_t row_pins[ROWS] = {26, 19, 13, 6};  // R1-R4
const uint8_t col_pins[COLS] = {16, 20, 21};     // C1-C3

// Key mapping
const char keymap[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

const int keymap_num[ROWS][COLS] = {
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9},
    {10, 0, 11}
};

void initialize_keypad() {
    if (!bcm2835_init()) {
        printf("Failed to initialize bcm2835!\n");
        return;
    }

    // Set rows as outputs (HIGH = inactive)
    for (int i = 0; i < ROWS; i++) {
        bcm2835_gpio_fsel(row_pins[i], BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_write(row_pins[i], HIGH);
    }

    // Set columns as inputs with pull-ups
    for (int i = 0; i < COLS; i++) {
        bcm2835_gpio_fsel(col_pins[i], BCM2835_GPIO_FSEL_INPT);
        bcm2835_gpio_set_pud(col_pins[i], BCM2835_GPIO_PUD_UP);
    }
}

char detect_key_press() {
    for (int row = 0; row < ROWS; row++) {
        bcm2835_gpio_write(row_pins[row], LOW);
        bcm2835_delayMicroseconds(100);  // Settling time
        
        for (int col = 0; col < COLS; col++) {
            if (bcm2835_gpio_lev(col_pins[col]) == LOW) {
                // Wait for key release
                while (bcm2835_gpio_lev(col_pins[col]) == LOW) {
                    bcm2835_delayMicroseconds(100);
                }
                bcm2835_gpio_write(row_pins[row], HIGH);
                return keymap[row][col];
            }
        }
        bcm2835_gpio_write(row_pins[row], HIGH);
    }
    return 0;
}

int get_key_press() {

    int ret_val = -1;

    for (int row = 0; row < ROWS; row++) {
        bcm2835_gpio_write(row_pins[row], LOW);
        bcm2835_delayMicroseconds(100);  // Settling time
        
        for (int col = 0; col < COLS; col++) {
            if (bcm2835_gpio_lev(col_pins[col]) == LOW) {
                // Wait for key release
                while (bcm2835_gpio_lev(col_pins[col]) == LOW) {
                    bcm2835_delayMicroseconds(100);
                }
                bcm2835_gpio_write(row_pins[row], HIGH);
                ret_val = keymap_num[row][col];
            }
        }
        bcm2835_gpio_write(row_pins[row], HIGH);
    }
    return ret_val;
}
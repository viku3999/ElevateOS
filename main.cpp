// Change from rpi
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <semaphore.h>
#include <functional>
#include <thread>
#include <vector>
#include <sched.h>

#include <signal.h>
#include <unistd.h>
#include <chrono>
 
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <syslog.h>

#include <fstream>

#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "i2c.h"
#include "oled.h"
#include "Sequencer.hpp"
#include "GPIO.hpp"
#include "keypad.h"

// Pin definitions
#define IR_DOOR_PIN 17 
#define IR_OBSTACLE_PIN 27

#define BUTTON_PIN_F1 11
#define BUTTON_PIN_F2 9
#define BUTTON_PIN_F3 10
#define BUTTON_PIN_F4 22

// System macro definitions
// Service 1
#define ELEVATOR_DOOR_OPEN_TIME 120 // ticks: 1tick = 25ms => 3seconds (for S1)
#define ELEVATOR_DOOR_OBSTACLE_TIME 800 // ticks: 1tick = 25ms => 20seconds (for S1)

// Service 2
#define ELEVATOR_MOVE_TIME 30 // ticks: 1tick = 100ms => 3seconds (for S2)

// Service 3
#define OLED_LINE_1 0
#define OLED_LINE_2 16
#define OLED_LINE_3 32
#define OLED_LINE_4 48

// System state definitions
#define ELEVATOR_DOOR_CLOSE 0
#define ELEVATOR_DOOR_OPEN 1
#define ELEVATOR_DOOR_CLOSING 2
#define ELEVATOR_DOOR_OPENING 3

#define IR_DETECTED 0
#define IR_NOT_DETECTED 1

#define ELEVATOR_FLOOR_SELECTION 0
#define ELEVATOR_MOVE 1

// Global shared variables
struct Elevator {
    int curr_floor; // Current floor
    int dest_floor; // Destination floor
    int door_status; // Door open/close status
    int door_open_flag; // Flag to indicate if door should be opened
    int floor_status; //Flag to indicate status of floor selection
};
Elevator elevator = {1, 1, ELEVATOR_DOOR_CLOSE, 0, ELEVATOR_FLOOR_SELECTION};

int _running = 1;  // Global variable to control service execution

Sequencer sequencer{};
// static int exit_flag = 0;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    printf("\nCaught signal %d (Ctrl+C). Exiting...\n", sig);
    syslog(LOG_CRIT, "Caught signal %d (Ctrl+C). Exiting...\n", sig);
    sequencer.stopServices();
    // exit_flag = 1; // Set exit flag to indicate termination
}

// Function to check GPIO values for all connected push buttons and IR sensors
void GPIO_Check(){
    static int init = 0;
    std::cout << "\n\tGPIO_CHECKS" << std::endl;
    if(!init){
        GPIO_Init(IR_DOOR_PIN, GPIO_IN);
        GPIO_Init(IR_OBSTACLE_PIN, GPIO_IN);
        GPIO_Init(BUTTON_PIN_F1, GPIO_IN);
        GPIO_Init(BUTTON_PIN_F2, GPIO_IN);
        GPIO_Init(BUTTON_PIN_F3, GPIO_IN);
        GPIO_Init(BUTTON_PIN_F4, GPIO_IN);

        init = 1;
    }

    int gpio_value = 0;

    GPIO_Get_Value(IR_DOOR_PIN, &gpio_value);
    std::cout << "IR_DOOR_PIN value: " << gpio_value << std::endl;
    GPIO_Get_Value(IR_OBSTACLE_PIN, &gpio_value);
    std::cout << "IR_OBSTACLE_PIN value: " << gpio_value << std::endl;

    GPIO_Get_Value(BUTTON_PIN_F1, &gpio_value);
    std::cout << "BUTTON_PIN_F1 value: " << gpio_value << std::endl;
    GPIO_Get_Value(BUTTON_PIN_F2, &gpio_value);
    std::cout << "BUTTON_PIN_F2 value: " << gpio_value << std::endl;
    GPIO_Get_Value(BUTTON_PIN_F3, &gpio_value);
    std::cout << "BUTTON_PIN_F3 value: " << gpio_value << std::endl;
    GPIO_Get_Value(BUTTON_PIN_F4, &gpio_value);
    std::cout << "BUTTON_PIN_F4 value: " << gpio_value << std::endl;
}

// Function to check OLED display
void OLED_Check() {
    int fd = i2c_init();
    if (fd < 0) {
        fprintf(stderr, "Failed to initialize I2C\n");
    }

    if (!SSD1106_init(fd)) {
        fprintf(stderr, "Failed to initialize OLED\n");
        i2c_close(fd);
    }

    // Display test pattern
    // SSD1106_clear_screen(fd);
    SSD1106_gotoXY(0, 0);
    SSD1106_puts(fd, "RPi OLED Test", &Font_7x10, SSD1106_COLOR_WHITE);
    
    SSD1106_gotoXY(0, 16);
    SSD1106_puts(fd, "7x10 Font Demo", &Font_7x10, SSD1106_COLOR_WHITE);
    
    SSD1106_gotoXY(0, 32);
    SSD1106_puts(fd, "ABCDEFGHIJKLM", &Font_7x10, SSD1106_COLOR_WHITE);
    
    SSD1106_gotoXY(0, 48);
    SSD1106_puts(fd, "NOPQRSTUVWXYZ", &Font_7x10, SSD1106_COLOR_WHITE);
    
    SSD1106_gotoXY(11, 48);
    SSD1106_printDigit(fd, 3, &Font_7x10, SSD1106_COLOR_WHITE);

    SSD1106_update_screen(fd);
    
    // Clear display before exiting
    // SSD1106_clear_screen(fd);
    i2c_close(fd);
}

// Function to check keypad input
void Keypad_Check() {
    initialize_keypad();

    int key_num = get_key_press();

    if(key_num>-1){
        std::cout << "Key number: " << key_num << std::endl;
    }
    
    bcm2835_close();
}

// Function to get the floor number from the push buttons
int get_push_button_floor() {
    int floor = 0;
    int button_value = 0;
    GPIO_Get_Value(BUTTON_PIN_F1, &button_value);
    if (button_value == 1) {
        floor = 1;
    } 
    else {
        GPIO_Get_Value(BUTTON_PIN_F2, &button_value);
        if (button_value == 1) {
            floor = 2;
        } 
        else {
            GPIO_Get_Value(BUTTON_PIN_F3, &button_value);
            if (button_value == 1) {
                floor = 3;
            } 
            else {
                GPIO_Get_Value(BUTTON_PIN_F4, &button_value);
                if (button_value == 1) {
                    floor = 4;
                }
            }
        }
    }
    return floor;
}

// Function to get the floor number from the keypad
int get_keypad_button_floor(){
    int init = 0;
    if(!init){
        initialize_keypad();
        init = 1;
    }

    return get_key_press();
}

/* Controls the elevator door
 * This service runs at 50ms intervals
 * Checks if the door_open_flag is set. if set, it opens the door for 
 * 'ELEVATOR_DOOR_OPEN_TIME' ticks. If an obstacle is detected using 
 * the IR_PERSON_PIN, it opens the door for 'ELEVATOR_DOOR_OBSTACLE_TIME' ticks.
 * Once the timer expires, it closes the door. Door is said to be closed once 
 * the input from IR_DOOR_PIN is 1 and then the appropriate variables are set.
 */
void Service_1(){
    static int init = 0;
    struct Elevator elevator_cpy = {0, 0, 0, 0};
    static int tick_count = 0;

    if(!init){
        GPIO_Init(IR_DOOR_PIN, GPIO_IN);
        GPIO_Init(IR_OBSTACLE_PIN, GPIO_IN);
        init = 1;
    }

    elevator_cpy = elevator; // Copy the shared elevator structure

    switch (elevator_cpy.door_status) {
        case ELEVATOR_DOOR_CLOSE:
            if (elevator_cpy.door_open_flag == 1) {
                // Open the door
                elevator_cpy.door_status = ELEVATOR_DOOR_OPENING;
                elevator = elevator_cpy; // Update the shared elevator structure
            }
            break;
    
        case ELEVATOR_DOOR_OPENING:
            if (elevator_cpy.door_open_flag == 1) {
                int temp = 0;
                GPIO_Get_Value(IR_DOOR_PIN, &temp);
                if (temp == IR_NOT_DETECTED) {
                    // Door is opened
                    elevator_cpy.door_status = ELEVATOR_DOOR_OPEN;
                    elevator_cpy.door_open_flag = 0; // Reset the door open flag
                    tick_count = ELEVATOR_DOOR_OPEN_TIME; // Set the timer for door open time
                    elevator = elevator_cpy; // Update the shared elevator structure
                }
            }
            break;
    
        case ELEVATOR_DOOR_OPEN:
            tick_count--;
            if (tick_count <= 0) {
                // Close the door
                elevator_cpy.door_status = ELEVATOR_DOOR_CLOSING;
                elevator = elevator_cpy; // Update the shared elevator structure
            } 
            else {
                // Check for obstacle
                int obstacle_value = 0;
                GPIO_Get_Value(IR_OBSTACLE_PIN, &obstacle_value);
                if (obstacle_value == IR_DETECTED) {
                    // Obstacle detected
                    tick_count = ELEVATOR_DOOR_OBSTACLE_TIME; // Set the timer for door open time
                    elevator = elevator_cpy; // Update the shared elevator structure
                }
            }
            break;
    
        case ELEVATOR_DOOR_CLOSING:
            if (tick_count <= 0) {
                int temp = 0;
                GPIO_Get_Value(IR_DOOR_PIN, &temp);
                if (temp == IR_DETECTED) {
                    // Door is closed
                    elevator_cpy.door_status = ELEVATOR_DOOR_CLOSE;
                    elevator = elevator_cpy; // Update the shared elevator structure
                }

                GPIO_Get_Value(IR_OBSTACLE_PIN, &temp);
                if(temp == IR_DETECTED) {
                    // Obstacle detected
                    elevator_cpy.door_status = ELEVATOR_DOOR_OPEN;
                    tick_count = ELEVATOR_DOOR_OBSTACLE_TIME; // Set the timer for door open time

                    elevator = elevator_cpy; // Update the shared elevator structure
                }
            }
            break;
    
        default:
            // Handle unexpected door status
            syslog(LOG_CRIT, "Service 1: Unexpected door status\n");
            break;
    }
}

/* Control the elevator movement
 * This service runs at 100ms intervals 
 */
void Service_2(){
    static int init = 0;
    struct Elevator elevator_cpy2 = {0, 0, 0, 0, 0};
    static int tick_count = 0;

    if(!init){
        GPIO_Init(BUTTON_PIN_F1, GPIO_IN);
        GPIO_Init(BUTTON_PIN_F2, GPIO_IN);
        GPIO_Init(BUTTON_PIN_F3, GPIO_IN);
        GPIO_Init(BUTTON_PIN_F4, GPIO_IN);
        //keypad init
        init = 1;
    }
    // mutex lock
    elevator_cpy2 = elevator; // Copy the shared elevator structure
    // mutex unlock 

    // If the door is closed, floor can be selected
    if(elevator_cpy2.door_status == ELEVATOR_DOOR_CLOSE) {
        int elevator_button_detected;
        int push_button_detected;
        switch (elevator_cpy2.floor_status) {
            case ELEVATOR_FLOOR_SELECTION:
                //Detect if keypad button is pressed if the elevator is at the destination floor
                elevator_button_detected = get_keypad_button_floor();
                push_button_detected = get_push_button_floor();

                if((elevator_button_detected) && (elevator_cpy2.curr_floor == elevator_cpy2.dest_floor) && (elevator_button_detected != -1)){
                    if((elevator_button_detected > 4) || (elevator_button_detected < 0)){
                        break;
                    }
                    elevator_cpy2.dest_floor = elevator_button_detected;
                }
                //Detect if push button is pressed if the elevator is at the destination floor
                else if((push_button_detected) && (elevator_cpy2.curr_floor == elevator_cpy2.dest_floor)){
                    elevator_cpy2.dest_floor = push_button_detected;
                }

                //If user selects same floor, break out
                if(elevator_cpy2.curr_floor == elevator_cpy2.dest_floor)
                    break;
                
                elevator_cpy2.floor_status = ELEVATOR_MOVE;
                
                //mutex_lock
                elevator = elevator_cpy2; // Update the shared elevator structure
                //mutex_unlock
            
                tick_count = ELEVATOR_MOVE_TIME; // Set the timer for floor move time
                
                break;

            case ELEVATOR_MOVE:
                tick_count -= 1;
                if(tick_count <= 0){
                    // Determine direction of movement and do ++ or --
                    elevator_cpy2.curr_floor = (elevator_cpy2.curr_floor > elevator_cpy2.dest_floor) ? elevator_cpy2.curr_floor - 1 : elevator_cpy2.curr_floor + 1;
                    
                    // reset the tick timer if the destination is not reached
                    if(elevator_cpy2.dest_floor != elevator_cpy2.curr_floor){
                        tick_count = ELEVATOR_MOVE_TIME; 
                    }
                    // If the elevator reaches the destination floor, open the door
                    else{
                        elevator_cpy2.floor_status = ELEVATOR_FLOOR_SELECTION;
                        elevator_cpy2.door_open_flag = 1; //Floor is reached so door can open
                    }
                }

                //mutex lock
                elevator = elevator_cpy2; // Update the shared elevator structure
                //mutex_unlock
                
                break;

            default:
                // Handle unexpected door status
                syslog(LOG_CRIT, "Service 2: Unexpected floor movement\n");
                break;
        }
    }
}

int fd;

void LCD_Init(){
    syslog(LOG_CRIT, "LCD init - init start\n");
    fd = i2c_init();
    if (fd < 0) {
        fprintf(stderr, "Failed to initialize I2C\n");
    }

    syslog(LOG_CRIT, "LCD init - I2C initialized\n");
    
    if (!SSD1106_init(fd)) {
        fprintf(stderr, "Failed to initialize OLED\n");
        i2c_close(fd);
    }
    syslog(LOG_CRIT, "LCD init - Disp initialized\n");
}

/* Control the OLED display and display the current floor, destination floor, and door status
 * This service runs at 150ms intervals
 */
void Service_3(){
    struct Elevator elevator_cpy3 = {0, 0, 0, 0, 0};
    // static int fd, init = 0;
    
    // mutex lock
    elevator_cpy3 = elevator; // Copy the shared elevator structure
    // mutex unlock 

    SSD1106_gotoXY(2, OLED_LINE_1);

    SSD1106_puts(fd, "Door is ", &Font_7x10, SSD1106_COLOR_WHITE);

    if(elevator_cpy3.door_status == ELEVATOR_DOOR_OPEN){
        SSD1106_puts(fd, "Open   ", &Font_7x10, SSD1106_COLOR_WHITE);
    }
    else if (elevator_cpy3.door_status == ELEVATOR_DOOR_OPENING){
        SSD1106_puts(fd, "Opening", &Font_7x10, SSD1106_COLOR_WHITE);
    }
    else if (elevator_cpy3.door_status == ELEVATOR_DOOR_CLOSING){
        SSD1106_puts(fd, "Closing", &Font_7x10, SSD1106_COLOR_WHITE);
    }
    else if (elevator_cpy3.door_status == ELEVATOR_DOOR_CLOSE){
        SSD1106_puts(fd, "Closed ", &Font_7x10, SSD1106_COLOR_WHITE);
    }
    else{
        SSD1106_puts(fd, "Unknown", &Font_7x10, SSD1106_COLOR_WHITE);
    }

    uint8_t digit = elevator_cpy3.curr_floor; //mutex lock?
    SSD1106_gotoXY(2, OLED_LINE_2);
    SSD1106_puts(fd, "Current Floor: ", &Font_7x10, SSD1106_COLOR_WHITE);
    SSD1106_printDigit(fd, digit, &Font_7x10, SSD1106_COLOR_WHITE);

    uint8_t digit1 = elevator_cpy3.dest_floor; //mutex lock?
    SSD1106_gotoXY(2, OLED_LINE_3);
    SSD1106_puts(fd, "Dest Floor: ", &Font_7x10, SSD1106_COLOR_WHITE);
    SSD1106_printDigit(fd, digit1, &Font_7x10, SSD1106_COLOR_WHITE);

    SSD1106_update_screen(fd);
}

// Main function
int main(int argc, char* argv[]) {
    syslog(LOG_CRIT, "<Start_TAG>\n");
    syslog(LOG_CRIT, "RTES Course Project\nDone by: Sriya Garde, and Vishnu Kumar\n");

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " -f <flag_value=1-4>" << std::endl;
        std::cerr <<" 1 - Elevator services\n 2 - GPIO Check\n 3 - OLED Check\n 4 - Keypad Check" << std::endl;
        std::cerr << "Please provide a flag value to run the appropriate service." << std::endl;
        return 1;
    }

    // Parse command-line arguments
    int flag_value = 0; // Default value
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            flag_value = std::atoi(argv[i + 1]);
            ++i; // Skip the value
        }
    }

    std::cout << "Flag value: " << flag_value << std::endl;
    
    // Add services based on the flag value
    switch(flag_value) {
        case 1: // Elevator services
            LCD_Init();
            sequencer.addService(&Service_1, 1, 99, 25);
            sequencer.addService(&Service_2, 1, 98, 50);
            sequencer.addService(&Service_3, 2, 99, 200);
            break;
        case 2:
            sequencer.addService(&GPIO_Check, 1, 99, 100);
            break;
            break;
        case 3:
            sequencer.addService(&OLED_Check, 1, 99, 1000);
            break;
        case 4:
            sequencer.addService(&Keypad_Check, 1, 99, 100);
            break;
        default:
            std::cerr << "Invalid flag value. Use -f <flag_value=1-4>" << std::endl;
            std::cerr <<" 1 - Elevator services\n 2 - GPIO Check\n 3 - OLED Check\n 4 - Keypad Check" << std::endl;
            std::cerr << "Please provide a flag value to run the appropriate service." << std::endl;
            return 1;
    }

    std::cout << "Services added\n";
    
    // Wait for ctrl-c or some other terminating condition
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0; // No special flags
    sigemptyset(&sa.sa_mask); // No blocked signals

    // Register signal handler
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL); // Handle termination signal

    sequencer.startServices();

    return 0;
}
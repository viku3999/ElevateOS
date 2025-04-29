// Change from rpi
#include <iostream>

#include "Sequencer.hpp"
#include "GPIO.hpp"

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

// Pin definitions
#define IR_DOOR_PIN 17 
#define IR_OBSTACLE_PIN 27

#define BUTTON_PIN_F1 11
#define BUTTON_PIN_F2 9
#define BUTTON_PIN_F3 10
#define BUTTON_PIN_F4 22

int _running = 1;  // Global variable to control service execution

Sequencer sequencer{};
static int exit_flag = 0;

void handle_sigint(int sig) {
    printf("\nCaught signal %d (Ctrl+C). Exiting...\n", sig);
    syslog(LOG_CRIT, "Caught signal %d (Ctrl+C). Exiting...\n", sig);
    sequencer.stopServices();
    exit_flag = 1; // Set exit flag to indicate termination
}

// Toggle GPIO 17
void GPIO_Toggle(){
    static int init = 0;
    if(!init){
        GPIO_Init(17, GPIO_OUT);
        init = 1;
    }
    static int gpio_value = 0;
    if(gpio_value == 0) {
        gpio_value = 1;
        GPIO_Set_Value(17, GPIO_HIGH);
        syslog(LOG_CRIT, "GPIO Task 4: GPIO17 value set to 1\n");
    } 
    else {
        gpio_value = 0;
        GPIO_Set_Value(17, GPIO_LOW);
        syslog(LOG_CRIT, "GPIO Task 4: GPIO17 value set to 0\n");
    }
}

// Toggle GPIO 17 based on GPIO 27 value
void GPIO_Toggle_IP(){
    static int init = 0;
    if(!init){
        GPIO_Init(17, GPIO_OUT);
        GPIO_Init(27, GPIO_IN);
        init = 1;
    }
    int gpio_value = 0;
    GPIO_Get_Value(27, &gpio_value);
    if(gpio_value == 0) {
        gpio_value = 1;
        GPIO_Set_Value(17, GPIO_HIGH);
        syslog(LOG_CRIT, "GPIO Task 7: GPIO17 value set to 1\n");
    } 
    else {
        gpio_value = 0;
        GPIO_Set_Value(17, GPIO_LOW);
        syslog(LOG_CRIT, "GPIO Task 7: GPIO17 value set to 0\n");
    }
}

void GPIO_CHECKS(){
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

// System macro definitions
// Service 1
#define ELEVATOR_DOOR_OPEN_TIME 60 // ticks: 1tick = 50ms => 3seconds (for S1)
#define ELEVATOR_DOOR_OBSTACLE_TIME 400 // ticks: 1tick = 50ms => 20seconds (for S1)

// Service 2
#define ELEVATOR_MOVE_TIME 30 // ticks: 1tick = 100ms => 3seconds (for S2)

// Service 3

// Local macro definitions
#define ELEVATOR_DOOR_CLOSE 0
#define ELEVATOR_DOOR_OPEN 1
#define ELEVATOR_DOOR_CLOSING 2
#define ELEVATOR_DOOR_OPENING 3

#define IR_DETECTED 0
#define IR_NOT_DETECTED 1

// Global shared variables
struct Elevator {
    int curr_floor; // Current floor
    int dest_floor; // Destination floor
    int door_status; // Door open/close status
    int door_open_flag; // Flag to indicate if door should be opened
};
Elevator elevator = {0, 0, 0, 0};

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

    /* 
    if((elevator_cpy.door_status == ELEVATOR_DOOR_CLOSE) && (elevator_cpy.door_open_flag == 1)) {
        // Open the door
        syslog(LOG_CRIT, "Service 1: Door is opening\n");
        elevator_cpy.door_status = ELEVATOR_DOOR_OPENING;
        
        elevator = elevator_cpy; // Update the shared elevator structure
    }
    else if ((elevator_cpy.door_status == ELEVATOR_DOOR_OPENING) && (elevator_cpy.door_open_flag == 1)) {
        int temp = 0;
        GPIO_Get_Value(IR_DOOR_PIN, &temp);
        if(temp == 0){
            // Door is opened
            syslog(LOG_CRIT, "Service 1: Door is opened\n");
            elevator_cpy.door_status = ELEVATOR_DOOR_OPEN;
            elevator_cpy.door_open_flag = 0; // Reset the door open flag
            tick_count = ELEVATOR_DOOR_OPEN_TIME; // Set the timer for door open time

            elevator = elevator_cpy; // Update the shared elevator structure
        }
    }
    else if(elevator_cpy.door_status == ELEVATOR_DOOR_OPEN) {
        tick_count--;
        if(tick_count <= 0) {
            // Close the door
            syslog(LOG_CRIT, "Service 1: Door is closing\n");
            elevator_cpy.door_status = ELEVATOR_DOOR_CLOSING;

            elevator = elevator_cpy; // Update the shared elevator structure
        }
        else{
            // Check for obstacle
            int obstacle_value = 0;
            GPIO_Get_Value(IR_OBSTACLE_PIN, &obstacle_value);
            if(obstacle_value == 1) {
                // Obstacle detected
                syslog(LOG_CRIT, "Service 1: Obstacle detected. Door will remain open\n");
                tick_count = ELEVATOR_DOOR_OBSTACLE_TIME; // Set the timer for door open time

                elevator = elevator_cpy; // Update the shared elevator structure
            }
        }
    }
    else if ((elevator_cpy.door_status == ELEVATOR_DOOR_CLOSING) && (tick_count <= 0)) {
        int temp = 0;
        GPIO_Get_Value(IR_DOOR_PIN, &temp);
        if(temp == 1){
            // Door is closed
            syslog(LOG_CRIT, "Service 1: Door is closed\n");
            elevator_cpy.door_status = ELEVATOR_DOOR_CLOSE;

            elevator = elevator_cpy; // Update the shared elevator structure
        }
    } 
    */

    switch (elevator_cpy.door_status) {
        case ELEVATOR_DOOR_CLOSE:
            if (elevator_cpy.door_open_flag == 1) {
                // Open the door
                syslog(LOG_CRIT, "Service 1: Door is opening\n");
                elevator_cpy.door_status = ELEVATOR_DOOR_OPENING;
                elevator = elevator_cpy; // Update the shared elevator structure
            }
            break;
    
        case ELEVATOR_DOOR_OPENING:
            if (elevator_cpy.door_open_flag == 1) {
                int temp = 0;
                GPIO_Get_Value(IR_DOOR_PIN, &temp);
                if (temp == IR_DETECTED) {
                    // Door is opened
                    syslog(LOG_CRIT, "Service 1: Door is opened\n");
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
                syslog(LOG_CRIT, "Service 1: Door is closing\n");
                elevator_cpy.door_status = ELEVATOR_DOOR_CLOSING;
                elevator = elevator_cpy; // Update the shared elevator structure
            } 
            else {
                // Check for obstacle
                int obstacle_value = 0;
                GPIO_Get_Value(IR_OBSTACLE_PIN, &obstacle_value);
                if (obstacle_value == IR_DETECTED1) {
                    // Obstacle detected
                    syslog(LOG_CRIT, "Service 1: Obstacle detected. Door will remain open\n");
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
                    syslog(LOG_CRIT, "Service 1: Door is closed\n");
                    elevator_cpy.door_status = ELEVATOR_DOOR_CLOSE;
                    elevator = elevator_cpy; // Update the shared elevator structure
                }

                GPIO_Get_Value(IR_OBSTACLE_PIN, &temp);
                if(temp == IR_DETECTED) {
                    // Obstacle detected
                    syslog(LOG_CRIT, "Service 1: Obstacle detected. Door will open again\n");
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

/* 
 * Control the elevator movement
 * This service runs at 100ms intervals 
 * 
 */
void Service_2(){

}

// Control the OLED display and display the current floor, destination floor, and door status
// This service runs at 150ms intervals
void Service_3(){

}

int main(int argc, char* argv[]) {
    syslog(LOG_CRIT, "RTES Course Project\nDone by: Sriya Garde, and Vishnu Kumar\n");

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " -f <flag_value=1-3>" << std::endl;
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
        case 0: // Elevator services
            sequencer.addService(&GPIO_Toggle_IP, 1, 98, 100);
            break;
        case 1:
            sequencer.addService(&GPIO_Toggle, 1, 98, 100);
            break;
        case 2:
            sequencer.addService(&GPIO_Toggle_IP, 1, 98, 100);
            break;
        case 3:
            sequencer.addService(&GPIO_CHECKS, 1, 98, 100);
            break;
        default:
            std::cerr << "Invalid flag value. Use -f <flag_value=1-6>" << std::endl;
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
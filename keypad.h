/**
 * @file keypad.h
 * @brief 4x3 Matrix Keypad Driver using bcm2835
 */

 #ifndef KEYPAD_H
 #define KEYPAD_H
 
 #include <bcm2835.h>
 
 // Keypad configuration
 #define ROWS 4
 #define COLS 3
 
 // BCM GPIO pins
 extern const uint8_t row_pins[ROWS];
 extern const uint8_t col_pins[COLS];
 
 // Key mapping
 extern const char keymap[ROWS][COLS];
 
 /**
  * @brief Initialize keypad GPIO
  */
 void initialize_keypad();
 
 /**
  * @brief Detect a single key press
  * @return Pressed key character or 0 if none
  */
 char detect_key_press();
 
 int get_key_press();

 #endif // KEYPAD_H
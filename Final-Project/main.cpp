#include "mbed.h"
#include "LCDi2c.h"
#include <cstring>

// Pin and Peripheral Configuration
DigitalIn touchSensor(PTC3);       // Touch input
UnbufferedSerial pc(USBTX, USBRX, 9600);  // UART Serial
LCDi2c lcd(PTC11, PTC10, LCD20x4, 0x27);  // LCD I2C
Timer touchTimer;

// Constants
#define BUFFER_SIZE 256
#define SHORT_TOUCH_THRESHOLD 0.2f  // seconds

// Function Prototypes
void select_mode();
void run_touch_sensor_mode();
void run_esp32_cam_mode();

int main() {
    // Setup
    touchSensor.mode(PullUp);
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Select Input:");
    lcd.locate(0, 1);
    lcd.printf("Short: ESP32-CAM");
    lcd.locate(0, 2);
    lcd.printf("Long : Touch Mode");

    select_mode();  // Wait for user selection and enter mode
    
    while (true) {
        // Main loop left empty intentionally
    }
}

void select_mode() {
    bool lastTouchState = false;
    touchTimer.start();

    while (true) {
        bool currentTouchState = touchSensor.read();

        if (currentTouchState && !lastTouchState) {
            touchTimer.reset();
        }

        if (!currentTouchState && lastTouchState) {
            float duration = chrono::duration<float>(touchTimer.elapsed_time()).count();
            lcd.cls();

            if (duration < SHORT_TOUCH_THRESHOLD) {
                lcd.locate(0, 0);
                lcd.printf("ESP32-CAM Mode");
                run_esp32_cam_mode();
            } else {
                lcd.locate(0, 0);
                lcd.printf("Touch Sensor Mode");
                run_touch_sensor_mode();
            }
            break;  // Exit after selecting mode
        }

        lastTouchState = currentTouchState;
        ThisThread::sleep_for(10ms);
    }
}

void run_esp32_cam_mode() {
    char received_char;
    char received_data[BUFFER_SIZE] = {0};
    int index = 0;

    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Waiting for CAM...");

    while (true) {
        while (pc.readable()) {
            pc.read(&received_char, 1);
            received_data[index++] = received_char;

            if (received_char == '\n' || index >= BUFFER_SIZE - 1) {
                received_data[index] = '\0';
                lcd.cls();
                lcd.locate(0, 0);
                lcd.printf("ESP32 Msg:");
                lcd.locate(0, 1);
                lcd.printf("%.20s", received_data);  // Show first 20 chars
                index = 0;
            }
        }
        ThisThread::sleep_for(50ms);
    }
}

void run_touch_sensor_mode() {
    bool lastTouchState = false;
    touchTimer.reset();

    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Touch Input Mode");
    lcd.locate(0, 1);
    lcd.printf("Pattern:");
    int col = 0;

    while (true) {
        bool currentTouchState = touchSensor.read();

        if (currentTouchState && !lastTouchState) {
            touchTimer.reset();
        }

        if (!currentTouchState && lastTouchState) {
            float duration = chrono::duration<float>(touchTimer.elapsed_time()).count();

            lcd.locate(col++, 2);
            if (duration < SHORT_TOUCH_THRESHOLD) {
                lcd.printf(".");
            } else {
                lcd.printf("-");
            }

            if (col >= 20) {
                lcd.locate(0, 3);
                lcd.printf("-- Limit Reached --");
                col = 0;
                ThisThread::sleep_for(2s);
                lcd.cls();
                lcd.locate(0, 0);
                lcd.printf("Touch Input Mode");
                lcd.locate(0, 1);
                lcd.printf("Pattern:");
            }

            touchTimer.reset();
        }

        lastTouchState = currentTouchState;
        ThisThread::sleep_for(10ms);
    }
}

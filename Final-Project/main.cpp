#include "mbed.h"
#include "LCDi2c.h"
#include <cstring>

// LCD setup
LCDi2c lcd(PTC11, PTC10, LCD20x4, 0x27);

// Serial (USB) - for ESP32-CAM
BufferedSerial pc(USBTX, USBRX, 9600);

// Buzzer
DigitalOut buzzer(PTC16);

// Touch Sensor
DigitalIn touchSensor(PTC8);  
Timer touchTimer;

// Constants
#define SHORT_TOUCH_THRESHOLD 0.2f  // seconds
#define BUFFER_SIZE 81

// Buzzer helper
void beep(int times = 1, int duration_ms = 200, int gap_ms = 100) {
    for (int i = 0; i < times; ++i) {
        buzzer = 1;
        ThisThread::sleep_for(chrono::milliseconds(duration_ms));
        buzzer = 0;
        if (i < times - 1) {
            ThisThread::sleep_for(chrono::milliseconds(gap_ms));
        }
    }
}

// Function Prototypes
void select_mode();
void run_esp32_cam_mode();
void run_touch_sensor_mode();

int main() {
    touchSensor.mode(PullUp);
    lcd.cls();
    beep(1);
    select_mode();

    while (true) {
        // stays empty — handled in mode functions
    }
}

void select_mode() {
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Select Input Mode:");
    lcd.locate(0, 1);
    lcd.printf("Short : ESP32-CAM");
    lcd.locate(0, 2);
    lcd.printf("Long  : Touch Input");

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
            break;
        }

        lastTouchState = currentTouchState;
        ThisThread::sleep_for(10ms);
    }
}

void run_esp32_cam_mode() {
    char buffer[BUFFER_SIZE];
    int index = 0;

    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Waiting for CAM...");

    printf("ESP32-CAM mode active.\n");

    while (true) {
        if (pc.readable()) {
            char c;
            pc.read(&c, 1);

            if (c == '\n' || index >= sizeof(buffer) - 1) {
                buffer[index] = '\0';

                lcd.cls();
                int len = strlen(buffer);

                if (len == 0) {
                    lcd.locate(0, 0);
                    lcd.printf("Error: Empty msg");
                    printf("Received empty message.\n");
                    beep(2, 150, 100);
                } else {
                    printf("Received: %s\n", buffer);
                    for (int i = 0; i < len && i < 80; i += 20) {
                        int row = i / 20;
                        lcd.locate(0, row);

                        char line[21];
                        strncpy(line, &buffer[i], 20);
                        line[20] = '\0';

                        lcd.printf("%-20s", line);
                        ThisThread::sleep_for(chrono::milliseconds(500));
                    }
                    beep(1, 200);
                }

                index = 0;
            } else {
                buffer[index++] = c;
            }
        }
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
                beep(1, 50);
            } else {
                lcd.printf("-");
                beep(1, 100);
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

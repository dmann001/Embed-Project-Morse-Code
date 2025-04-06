#include "esp_camera.h"

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
#include "camera_pins.h" // Pin definitions for Wrover Kit

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32-CAM...");

  // Camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; // Power down pin
  config.pin_reset = RESET_GPIO_NUM; // Reset pin
  config.xclk_freq_hz = 20000000; // XCLK frequency
  config.pixel_format = PIXFORMAT_JPEG; // Output format: JPEG
  config.frame_size = FRAMESIZE_UXGA;   // Ultra-high resolution (1600x1200)
  config.jpeg_quality = 10;            // Higher quality (lower number)
  config.fb_count = psramFound() ? 2 : 1; // Use two frame buffers if PSRAM is available

  // Camera initialization
  Serial.println("Initializing camera...");
  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error code: %d\n", err);
    return; // Stop execution if camera initialization fails
  }

  Serial.println("Camera initialized successfully!");

  // Adjust sensor settings to fix green tint issue
  adjustSensorSettings();

  // Capture and send an image immediately
  captureAndSendImage();
}

void loop() {
}

// Function to adjust sensor settings
void adjustSensorSettings() {
  sensor_t *s = esp_camera_sensor_get();
  
  if (s != NULL) {
    s->set_brightness(s, 1);    // Brightness: -2 to +2
    s->set_contrast(s, 1);      // Contrast: -2 to +2
    s->set_saturation(s, -2);   // Saturation: -2 to +2
    s->set_whitebal(s, true);   // Enable/disable white balance
    s->set_awb_gain(s, true);   // Enable/disable AWB gain
    s->set_wb_mode(s, 0);       // White balance mode (e.g., auto)
    s->set_hmirror(s, false);   // Horizontal mirror (true/false)
    s->set_vflip(s, false);     // Vertical flip (true/false)
    s->set_special_effect(s, 0);// Special effects (e.g., none)
    Serial.println("Sensor settings adjusted!");
  } else {
    Serial.println("Failed to get camera sensor!");
  }
}

// Function to capture and send an image over serial communication
void captureAndSendImage() {
  camera_fb_t * fb = NULL;

  Serial.println("Capturing image...");
  
  fb = esp_camera_fb_get(); // Capture an image
  if (!fb) {
    Serial.println("Failed to capture image!");
    return;
  }

  Serial.printf("Image captured successfully! Size: %d bytes\n", fb->len);

  // Send image data over serial
  Serial.println("Sending image...");
  
  Serial.print("\n\nSTART_IMAGE\n");       // Start marker
  Serial.printf("SIZE:%d\n", fb->len);     // Image size marker
  Serial.write(fb->buf, fb->len);          // Raw JPEG data
  delay(50);                               // Allow time for transmission
  Serial.print("\nEND_IMAGE\n");           // End marker
  Serial.flush();                          // Ensure all data is sent

  Serial.println("Image sent successfully!");

  esp_camera_fb_return(fb);                // Release the frame buffer
}

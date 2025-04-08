import serial
import time
import os
import io
import base64
import mimetypes
import anthropic
from PIL import Image

# Configuration
ESP32_PORT = 'COM7' 
ESP32_BAUDRATE = 115200
ESP32_TIMEOUT = 10
CLAUDE_API_KEY = "API-KEY"  
CLAUDE_MODEL = "claude-3-7-sonnet-20250219"
OUTPUT_DIR = "captured_images"
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Initialize Serial connection
ser = serial.Serial(ESP32_PORT, baudrate=ESP32_BAUDRATE, timeout=ESP32_TIMEOUT)

# Initialize Anthropic client
client = anthropic.Anthropic(api_key=CLAUDE_API_KEY)

def capture_image_from_esp32():
    """Captures an image from ESP32-CAM and returns the processed image"""
    try:
        # Wait for start marker
        line = ""
        while "START_IMAGE" not in line:
            line = ser.readline().decode(errors='ignore').strip()
            print(f"Received line: {line}")

        print("Start marker detected")

        # Get size of image
        size_line = ser.readline().decode(errors='ignore').strip()
        print(f"Size line: {size_line}")

        if not size_line.startswith("SIZE:"):
            print("Invalid size marker - skipping")
            return None

        size = int(size_line.split(":")[1])
        print(f"Image size: {size} bytes")

        # Read exact number of bytes for the image data
        image_data = bytearray()
        while len(image_data) < size:
            chunk = ser.read(size - len(image_data))
            if not chunk:
                raise Exception("Timeout while reading image data")
            image_data.extend(chunk)

        print(f"Received {len(image_data)} bytes of image data")

        # Verify end marker
        end_line = ""
        while "END_IMAGE" not in end_line:
            end_line = ser.readline().decode(errors='ignore').strip()
            print(f"End line: {end_line}")

        if "END_IMAGE" not in end_line:
            print("Corrupted image - discarding")
            return None

        # Open and process the image using Pillow
        img = Image.open(io.BytesIO(image_data))
        
        # Apply mirror (horizontal flip) and rotate 180°
        img_processed = img.transpose(Image.FLIP_LEFT_RIGHT).rotate(180)
        
        # Save the processed image
        timestamp = time.strftime('%Y%m%d_%H%M%S')
        filename = os.path.join(OUTPUT_DIR, f"image_{timestamp}.jpg")
        img_processed.save(filename)
        print(f"Saved: {filename}")
        
        return filename
    
    except Exception as e:
        print(f"Error during image capture: {str(e)}")
        return None
    finally:
        # Clear residual data in serial buffer
        ser.reset_input_buffer()

def encode_image(image_path):
    """Encodes image to base64 and detects media type"""
    mime_type, _ = mimetypes.guess_type(image_path)
    if mime_type not in ["image/jpeg", "image/png"]:
        print(f"Warning: Unexpected mime type {mime_type}, defaulting to image/jpeg")
        mime_type = "image/jpeg"
    
    with open(image_path, "rb") as image_file:
        encoded_string = base64.b64encode(image_file.read()).decode("utf-8")
    
    return encoded_string, mime_type

def analyze_with_claude(image_path):
    """Sends image to Claude API with a fixed prompt and returns the response"""
    try:
        image_base64, media_type = encode_image(image_path)

        # Hardcoded prompt for analysis
        prompt_text = (
            "convert the following picture morse code into english letters. Interpret small squares as dots (.) and larger rectangles as dashes (-). Further use international morse code only. Dont hallucinate. Each row is a english letter."
        )

        # Create API request
        message = client.messages.create(
            model=CLAUDE_MODEL,
            max_tokens=1024,  # Adjust based on expected response length
            messages=[
                {
                    "role": "user",
                    "content": [
                        {
                            "type": "image",
                            "source": {
                                "type": "base64",
                                "media_type": media_type,
                                "data": image_base64,
                            },
                        },
                        {
                            "type": "text",
                            "text": prompt_text,  # Hardcoded prompt
                        },
                    ],
                }
            ],
        )

        # Return Claude's response
        return message.content[0].text if message.content else "Error: No response received"

    except Exception as e:
        print(f"Error analyzing image with Claude: {str(e)}")
        return f"Error: {str(e)}"


def main():
    print("ESP32-CAM Image Capture and Claude Analysis System")
    print("Press Ctrl+C to exit")
    
    # Default prompt for Claude (can be customized)
    default_prompt = "Describe what you see in this image in detail."
    
    try:
        while True:
            print("\n1. Capture image and analyze")
            print("2. Change analysis prompt")
            print("3. Exit")
            choice = input("Select an option (1-3): ")
            
            if choice == "1":
                print("Capturing image from ESP32-CAM...")
                image_path = capture_image_from_esp32()
                
                if image_path:
                    print(f"Image captured and saved to {image_path}")
                    print("Sending to Claude API for analysis...")
                    
                    response = analyze_with_claude(image_path)
                    print("\nClaude's Analysis:")
                    print(response)
                else:
                    print("Failed to capture image")
            
            elif choice == "2":
                new_prompt = input("Enter new prompt for Claude: ")
                if new_prompt:
                    default_prompt = new_prompt
                    print(f"Prompt updated to: {default_prompt}")
            
            elif choice == "3":
                print("Exiting program...")
                break
            
            else:
                print("Invalid option, please try again")
    
    except KeyboardInterrupt:
        print("\nProgram terminated by user")
    except Exception as e:
        print(f"Unexpected error: {str(e)}")
    finally:
        ser.close()
        print("Serial port closed")

if __name__ == "__main__":
    main()

import serial
import time
import os
import io
import base64
import mimetypes
import anthropic
from PIL import Image

ESP32_PORT = 'COM7'
ESP32_BAUDRATE = 115200
ESP32_TIMEOUT = 20

K66F_PORT = 'COM3'
K66F_BAUDRATE = 9600

CLAUDE_API_KEY = "API-KEY"
CLAUDE_MODEL = "claude-3-7-sonnet-20250219"

OUTPUT_DIR = "captured_images"
os.makedirs(OUTPUT_DIR, exist_ok=True)

esp32_ser = serial.Serial(ESP32_PORT, ESP32_BAUDRATE, timeout=ESP32_TIMEOUT)
k66f_ser = serial.Serial(K66F_PORT, K66F_BAUDRATE, timeout=2)

client = anthropic.Anthropic(api_key=CLAUDE_API_KEY)

def capture_image_from_esp32():
    try:
        line = ""
        print("Waiting for START_IMAGE...")
        while "START_IMAGE" not in line:
            line = esp32_ser.readline().decode(errors='ignore').strip()
            print(f"ESP32: {line}")

        size_line = esp32_ser.readline().decode(errors='ignore').strip()
        print(f"ESP32: {size_line}")

        if not size_line.startswith("SIZE:"):
            print("Invalid SIZE marker.")
            return None

        size = int(size_line.split(":")[1])
        print(f"Receiving image of {size} bytes.")

        image_data = bytearray()
        while len(image_data) < size:
            chunk = esp32_ser.read(size - len(image_data))
            if not chunk:
                raise TimeoutError("Timeout receiving image data.")
            image_data.extend(chunk)

        print("Image data received.")

        end_line = esp32_ser.readline().decode(errors='ignore').strip()
        print(f"ESP32: {end_line}")

        if end_line != "END_IMAGE":
            print("Missing END_IMAGE marker.")
            return None

        img = Image.open(io.BytesIO(image_data))
        img_processed = img.transpose(Image.FLIP_LEFT_RIGHT).rotate(180)
        filename = os.path.join(OUTPUT_DIR, f"image_{time.strftime('%Y%m%d_%H%M%S')}.jpg")
        img_processed.save(filename)
        print(f"Image saved: {filename}")
        return filename

    except Exception as e:
        print(f"Error capturing image: {e}")
        return None
    finally:
        esp32_ser.reset_input_buffer()

def encode_image(image_path):
    mime_type, _ = mimetypes.guess_type(image_path)
    with open(image_path, "rb") as f:
        encoded = base64.b64encode(f.read()).decode()
    return encoded, mime_type or "image/jpeg"

def analyze_with_claude(image_path):
    image_base64, media_type = encode_image(image_path)
    prompt = (
                              "detect the morse code dots(black filled circles) and dashes(black filled rectangles)--> print them from left to right line by line."

    )

    message = client.messages.create(
        model=CLAUDE_MODEL,
        max_tokens=1024,
        messages=[{
            "role": "user",
            "content": [
                {"type": "image", "source": {"type": "base64", "media_type": media_type, "data": image_base64}},
                {"type": "text", "text": prompt}
            ]
        }]
    )

    return message.content[0].text.strip() if message.content else "No response"

def extract_clean_message(raw_analysis: str) -> str:
    prompt = (
         "USE INTERNATIONAL MORSE CODE-->convert the following picture morse code into english letters. Read from Left to right and each line should be read as single english letter-ONLY use international morse code only. Dont hallucinate. Each row is a english letter. "
    "if no morse describe the short scene description (under 80 characters).\n"
    "Do not explain. Do not include labels. Just return the clean final text.\n\n"
    f"{raw_analysis}"
    )

    try:
        message = client.messages.create(
            model=CLAUDE_MODEL,
            max_tokens=50,
            messages=[{
                "role": "user",
                "content": [{"type": "text", "text": prompt}]
            }]
        )
        return message.content[0].text.strip() if message.content else "UNKNOWN"
    except Exception as e:
        print(f"Error extracting clean message: {e}")
        return "ERROR"

def send_to_k66f(message):
    message = message.strip() + "\n"
    k66f_ser.write(message.encode('utf-8'))
    print(f"Sent to K66F: {message.strip()}")

def main():
    print("Morse Code Image Translator System Running...")
    try:
        while True:
            image_path = capture_image_from_esp32()
            if image_path:
                raw_result = analyze_with_claude(image_path)
                print(f"Claude API Result (Raw):\n{raw_result}")

                clean_result = extract_clean_message(raw_result)
                print(f"Final Decoded Message: {clean_result}")

                send_to_k66f(clean_result)
            else:
                print("Image capture failed. Retrying...")
            time.sleep(5)
    except KeyboardInterrupt:
        print("Stopped by user.")
    finally:
        esp32_ser.close()
        k66f_ser.close()
        print("Serial connections closed.")

if __name__ == "__main__":
    main()

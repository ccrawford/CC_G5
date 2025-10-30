import sys
from PIL import Image
import os
import numpy as np
import math

def convert_to_1bit(image_path, threshold=128):
    try:
        # Open image and convert to binary (1-bit)
        img = Image.open(image_path)
        if img.mode != '1':
            # Convert to grayscale then apply threshold for binary
            img = img.convert('L')
            img = img.point(lambda x: 0 if x < threshold else 255, '1')
    except Exception as e:
        print(f"Error opening or converting image: {e}")
        sys.exit(1)
    
    # Extract the base filename without path and extension
    base_name = os.path.splitext(os.path.basename(image_path))[0].upper()
    
    width, height = img.size
    
    # Define the 1-bit palette in RGB565 (usually just black and white)
    # Colors are byte-swapped for ESP32
    black = ((0 & 0xF8) << 8) | ((0 & 0xFC) << 3) | (0 >> 3)
    white = ((255 & 0xF8) << 8) | ((255 & 0xFC) << 3) | (255 >> 3)
    swapped_black = ((black & 0xFF) << 8) | ((black >> 8) & 0xFF)
    swapped_white = ((white & 0xFF) << 8) | ((white >> 8) & 0xFF)
    
    # Calculate bytes needed for the bitmap data
    # Each byte stores 8 pixels
    bytes_per_row = math.ceil(width / 8)
    total_bytes = bytes_per_row * height
    
    # Get the raw pixel data and pack it
    pixel_data = list(img.getdata())
    packed_data = []
    
    for y in range(height):
        for x_byte in range(bytes_per_row):
            byte_val = 0
            for bit in range(8):
                x = x_byte * 8 + bit
                if x < width:
                    pixel_idx = y * width + x
                    # If pixel is white (255), bit is 1
                    if pixel_data[pixel_idx] > 0:
                        byte_val |= (1 << (7 - bit))  # MSB first format
            packed_data.append(byte_val)
    
    # Generate C header
    output = []
    output.append(f"// Generated 1-bit monochrome C header file\n")
    output.append(f"#define {base_name}_IMG_WIDTH {width}\n")
    output.append(f"#define {base_name}_IMG_HEIGHT {height}\n")
    output.append(f"#define {base_name}_BYTES_PER_ROW {bytes_per_row}\n\n")
    
    # Output the simple 2-color palette
    output.append(f"static constexpr uint16_t {base_name}_PALETTE[2] PROGMEM = {{\n")
    output.append(f"    0x{swapped_black:04X},  // Black (0)\n")
    output.append(f"    0x{swapped_white:04X}   // White (1)\n")
    output.append("};\n\n")
    
    # Output the packed 1-bit pixel data
    output.append(f"static constexpr uint8_t {base_name}_IMG_DATA[{total_bytes}] PROGMEM = {{\n    ")
    for i, byte_val in enumerate(packed_data):
        output.append(f"0x{byte_val:02X}, ")
        if (i + 1) % 16 == 0:
            output.append("\n    ")
        elif (i + 1) % bytes_per_row == 0:
            output.append("\n    ")
    output.append("\n};\n")
    
    return "".join(output)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python convert_to_1bit.py <image_file> [threshold]")
        print("       threshold (0-255) defaults to 128 if not specified")
        sys.exit(1)
    
    image_file = sys.argv[1]
    threshold = 128
    
    if len(sys.argv) >= 3:
        try:
            threshold = int(sys.argv[2])
            if threshold < 0 or threshold > 255:
                print("threshold must be between 0 and 255")
                sys.exit(1)
        except ValueError:
            print("threshold must be an integer")
            sys.exit(1)
    
    c_header = convert_to_1bit(image_file, threshold)
    
    # Output filename
    output_file = os.path.splitext(image_file)[0] + "_1bit.h"
    
    # Write to file
    with open(output_file, "w") as f:
        f.write(c_header)
    
    # print(f"Converted 1-bit image saved to {output_file}")
    print(c_header)
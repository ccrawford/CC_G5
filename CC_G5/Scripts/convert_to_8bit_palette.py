import sys
from PIL import Image
import os
import numpy as np

def convert_to_8bit_indexed(image_path, max_colors=256):
    try:
        # Open image and convert to paletted/indexed mode with specified color limit
        img = Image.open(image_path)
        if img.mode != 'P':
            # Convert to paletted mode (8-bit indexed color)
            img = img.convert('RGB').quantize(colors=max_colors)
    except Exception as e:
        print(f"Error opening or converting image: {e}")
        sys.exit(1)
    
    # Extract the base filename without path and extension
    base_name = os.path.splitext(os.path.basename(image_path))[0].upper()
    
    width, height = img.size
    
    # Get the palette and convert it to RGB565 format
    palette = img.getpalette()
    rgb565_palette = []
    
    for i in range(0, min(max_colors * 3, len(palette)), 3):
        r, g, b = palette[i], palette[i+1], palette[i+2]
        # Convert RGB888 to RGB565
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        # Swap bytes for little-endian MCUs
        swapped_rgb565 = ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)
        rgb565_palette.append(swapped_rgb565)
    
    # Fill the remaining palette entries if needed
    while len(rgb565_palette) < max_colors:
        rgb565_palette.append(0)
    
    # Get the pixel data (indices into the palette)
    pixel_data = np.array(img.getdata())
    
    # Generate C header
    output = []
    output.append(f"// Generated 8-bit indexed color C header file\n")
    output.append(f"#define {base_name}_IMG_WIDTH {width}\n")
    output.append(f"#define {base_name}_IMG_HEIGHT {height}\n")
    output.append(f"#define {base_name}_PALETTE_SIZE {len(rgb565_palette)}\n\n")
    
    # Output the palette
    output.append(f"static constexpr uint16_t {base_name}_PALETTE[{base_name}_PALETTE_SIZE] PROGMEM = {{\n    ")
    for i, color in enumerate(rgb565_palette):
        output.append(f"0x{color:04X}, ")
        if (i + 1) % 8 == 0:
            output.append("\n    ")
    output.append("\n};\n\n")
    
    # Output the 8-bit indexed pixel data
    output.append(f"static constexpr uint8_t {base_name}_IMG_DATA[{width} * {height}] PROGMEM = {{\n    ")
    for i, pixel in enumerate(pixel_data):
        output.append(f"0x{pixel:02X}, ")
        if (i + 1) % 16 == 0:
            output.append("\n    ")
        elif (i + 1) % width == 0:
            output.append("\n    ")
    output.append("\n};\n")
    
    return "".join(output)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python convert_to_8bit_indexed.py <image_file> [max_colors]")
        print("       max_colors defaults to 256 if not specified")
        sys.exit(1)
    
    image_file = sys.argv[1]
    max_colors = 256
    
    if len(sys.argv) >= 3:
        try:
            max_colors = int(sys.argv[2])
            if max_colors <= 0 or max_colors > 256:
                print("max_colors must be between 1 and 256")
                sys.exit(1)
        except ValueError:
            print("max_colors must be an integer")
            sys.exit(1)
    
    c_header = convert_to_8bit_indexed(image_file, max_colors)
    
    # Output filename
    output_file = os.path.splitext(image_file)[0] + "_8bit.h"
    
    # Write to file
    with open(output_file, "w") as f:
        f.write(c_header)
    
    #  print(f"Converted image saved to {output_file}")
    print(c_header)
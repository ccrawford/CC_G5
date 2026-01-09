import sys

def vlw_to_header(vlw_path, header_path, array_name):
    with open(vlw_path, 'rb') as f:
        data = f.read()
    
    with open(header_path, 'w') as f:
        f.write(f'#pragma once\n\n')
        f.write(f'const uint8_t {array_name}[] PROGMEM = {{\n')
        
        for i, byte in enumerate(data):
            if i % 16 == 0:
                f.write('    ')
            f.write(f'0x{byte:02X}, ')
            if i % 16 == 15:
                f.write('\n')
        
        f.write(f'\n}};\n')
        f.write(f'const size_t {array_name}_size = {len(data)};\n')

# Usage: python vlw_to_header.py PrimaSansBold24.vlw prima_sans_bold_24.h primaSansBold24
vlw_to_header(sys.argv[1], sys.argv[2], sys.argv[3])
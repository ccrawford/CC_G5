#!/usr/bin/env python3
"""
MobiFlight Configuration Duplicator

This script takes a MobiFlight ROOT CONFIG file and duplicates all device configurations
for a second device with a different serial number, creating an "All-In-One" (AIO) config.

Usage:
    python duplicate_mf_config.py <root_config.mcc> <device1_serial> <device2_serial> <output_file.mcc>

Example:
    python duplicate_mf_config.py "G5 ROOT CONFIG.mcc" "CC_G5 ESP32/ SN-98A316E591C4" "CC_G5 ESP32/ SN-ABCDEF123456" "G5 AIO.mcc"
"""

import sys
import xml.etree.ElementTree as ET
import uuid
from pathlib import Path


def generate_new_guid():
    """Generate a new GUID for duplicated config entries."""
    return str(uuid.uuid4())


def duplicate_config(config_elem, old_serial, new_serial, is_second_device=False):
    """
    Duplicate a config element, replacing the serial number and generating a new GUID.

    Args:
        config_elem: XML element representing a config entry
        old_serial: Original device serial number
        new_serial: New device serial number
        is_second_device: If True, rename ccDeviceID variable to ccDeviceID2

    Returns:
        Duplicated config element with updated serial and GUID
    """
    # Create a deep copy of the element
    new_config = ET.fromstring(ET.tostring(config_elem))

    # Generate and set new GUID
    new_guid = generate_new_guid()
    new_config.set('guid', new_guid)

    # Find and replace serial numbers in the settings
    settings = new_config.find('settings')
    if settings is not None:
        # Replace serial in display elements (outputs)
        display = settings.find('display')
        if display is not None and display.get('serial') == old_serial:
            display.set('serial', new_serial)

        # Replace serial attribute directly on settings (inputs)
        if settings.get('serial') == old_serial:
            settings.set('serial', new_serial)

        # If this is the second device, rename ccDeviceID variable references
        if is_second_device:
            # For inputs: rename varName in onPress elements
            for on_press in settings.findall('.//onPress[@varName="ccDeviceID"]'):
                on_press.set('varName', 'ccDeviceID2')

            # For outputs: rename ref in precondition elements
            for precondition in settings.findall('.//precondition[@ref="ccDeviceID"]'):
                precondition.set('ref', 'ccDeviceID2')

    return new_config


def duplicate_mf_config(input_file, device1_serial, device2_serial, output_file):
    """
    Duplicate MobiFlight configuration for a second device.

    Args:
        input_file: Path to the root config .mcc file
        device1_serial: Serial number of the first device (already in the config)
        device2_serial: Serial number of the second device (to be added)
        output_file: Path for the output .mcc file
    """
    # Parse the XML file
    tree = ET.parse(input_file)
    root = tree.getroot()

    # Track statistics
    stats = {
        'outputs_original': 0,
        'outputs_duplicated': 0,
        'inputs_original': 0,
        'inputs_duplicated': 0,
        'separators': 0
    }

    # Process outputs section
    outputs = root.find('outputs')
    if outputs is not None:
        original_configs = list(outputs.findall('config'))
        stats['outputs_original'] = len(original_configs)

        for config in original_configs:
            # Check if this config uses the device serial
            settings = config.find('settings')
            if settings is not None:
                display = settings.find('display')

                # Skip inactive separator entries
                active = config.find('active')
                if active is not None and active.text == 'false':
                    stats['separators'] += 1
                    continue

                # Duplicate if it references the device
                if display is not None and display.get('serial') == device1_serial:
                    new_config = duplicate_config(config, device1_serial, device2_serial, is_second_device=True)
                    outputs.append(new_config)
                    stats['outputs_duplicated'] += 1

    # Process inputs section
    inputs = root.find('inputs')
    if inputs is not None:
        original_configs = list(inputs.findall('config'))
        stats['inputs_original'] = len(original_configs)

        for config in original_configs:
            # Check if this config uses the device serial
            settings = config.find('settings')
            if settings is not None:
                # Skip inactive entries
                active = config.find('active')
                if active is not None and active.text == 'false':
                    continue

                # Duplicate if it references the device
                if settings.get('serial') == device1_serial:
                    new_config = duplicate_config(config, device1_serial, device2_serial, is_second_device=True)
                    inputs.append(new_config)
                    stats['inputs_duplicated'] += 1

    # Write the output file
    tree.write(output_file, encoding='UTF-8', xml_declaration=True)

    # Print statistics
    print(f"MobiFlight Configuration Duplication Complete!")
    print(f"=" * 60)
    print(f"Input file:  {input_file}")
    print(f"Output file: {output_file}")
    print(f"")
    print(f"Device 1 Serial: {device1_serial}")
    print(f"Device 2 Serial: {device2_serial}")
    print(f"")
    print(f"Statistics:")
    print(f"  Original outputs:    {stats['outputs_original']} (including {stats['separators']} separators)")
    print(f"  Duplicated outputs:  {stats['outputs_duplicated']}")
    print(f"  Original inputs:     {stats['inputs_original']}")
    print(f"  Duplicated inputs:   {stats['inputs_duplicated']}")
    print(f"  Total configs:       {stats['outputs_original'] + stats['outputs_duplicated'] + stats['inputs_original'] + stats['inputs_duplicated']}")


def main():
    """Main entry point for the script."""
    if len(sys.argv) != 5:
        print(__doc__)
        print("\nError: Incorrect number of arguments")
        sys.exit(1)

    input_file = Path(sys.argv[1])
    device1_serial = sys.argv[2]
    device2_serial = sys.argv[3]
    output_file = Path(sys.argv[4])

    if not input_file.exists():
        print(f"Error: Input file '{input_file}' does not exist")
        sys.exit(1)

    try:
        duplicate_mf_config(input_file, device1_serial, device2_serial, output_file)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()

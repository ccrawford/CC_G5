#!/usr/bin/env python3
"""
Dual G5 Configuration Creator

This script automates the creation of a dual G5 MobiFlight configuration.
It reads the ROOT CONFIG and duplicates all settings for both the UPPER (PFD) and LOWER (HSI) devices.

The script will:
1. Read the G5 ROOT CONFIG.mcc
2. Duplicate all configurations for the UPPER device (PFD)
3. Duplicate all configurations for the LOWER device (HSI)
4. Save the combined configuration as G5 AIO.mcc

Usage:
    python create_dual_g5_config.py
"""

import os
import sys
from pathlib import Path
import xml.etree.ElementTree as ET
import uuid


# Device serial numbers - use exact serial strings from the devices
UPPER_SERIAL = "CC_G5 ESP32/ SN-98A316E591C4"  # PFD
LOWER_SERIAL = "CC_G5 ESP32/ SN-28372F8A1B18"  # HSI

# File paths (relative to script location)
SCRIPT_DIR = Path(__file__).parent
MF_DIR = SCRIPT_DIR.parent.parent / "MF"
ROOT_CONFIG = MF_DIR / "G5 ROOT CONFIG.mcc"
OUTPUT_FILE = MF_DIR / "G5 AIO.mcc"


def generate_new_guid():
    """Generate a new GUID for duplicated config entries."""
    return str(uuid.uuid4())


def duplicate_config(config_elem, old_serial, new_serial):
    """
    Duplicate a config element, replacing the serial number (keeping original GUID).

    Args:
        config_elem: XML element representing a config entry
        old_serial: Original device serial number (e.g., "CC_G5 ESP32/ SN-98A316E591C4")
        new_serial: New device serial number (e.g., "CC_G5 ESP32/ SN-28372F8A1B18")

    Returns:
        Duplicated config element with updated serial (GUID unchanged)
    """
    # Create a deep copy of the element
    new_config = ET.fromstring(ET.tostring(config_elem))

    # Keep the original GUID - don't generate a new one
    # This preserves configref relationships between configs

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

    return new_config


def create_dual_g5_config():
    """
    Create a dual G5 configuration by duplicating the ROOT CONFIG for both devices.
    """
    print("=" * 70)
    print("Dual G5 Configuration Creator")
    print("=" * 70)
    print()

    # Verify input file exists
    if not ROOT_CONFIG.exists():
        print(f"ERROR: Root config file not found at: {ROOT_CONFIG}")
        print(f"       Please ensure the file exists and try again.")
        sys.exit(1)

    print(f"Reading ROOT CONFIG from: {ROOT_CONFIG}")
    print()

    # Parse the XML file
    tree = ET.parse(ROOT_CONFIG)
    root = tree.getroot()

    # Get the original serial from ROOT CONFIG
    original_serial = None
    outputs = root.find('outputs')
    if outputs is not None:
        for config in outputs.findall('config'):
            settings = config.find('settings')
            if settings is not None:
                display = settings.find('display')
                if display is not None:
                    serial = display.get('serial')
                    if serial and serial.startswith("CC_G5"):
                        original_serial = serial
                        break

    if not original_serial:
        print("ERROR: Could not find original device serial in ROOT CONFIG")
        sys.exit(1)

    print(f"Original device serial: {original_serial}")
    print()
    print(f"Creating dual configuration:")
    print(f"  UPPER (PFD): {UPPER_SERIAL}")
    print(f"  LOWER (HSI): {LOWER_SERIAL}")
    print()

    # Track statistics
    stats = {
        'outputs_original': 0,
        'outputs_upper': 0,
        'outputs_lower': 0,
        'inputs_original': 0,
        'inputs_upper': 0,
        'inputs_lower': 0,
        'separators': 0
    }

    # Process outputs section - clear all existing entries first
    if outputs is not None:
        # Remove all existing configs
        for config in list(outputs.findall('config')):
            outputs.remove(config)

        # Parse the ROOT CONFIG fresh to get original configs
        root_tree = ET.parse(ROOT_CONFIG)
        root_root = root_tree.getroot()
        root_outputs = root_root.find('outputs')

        if root_outputs is not None:
            original_configs = list(root_outputs.findall('config'))
            stats['outputs_original'] = len(original_configs)

            for config in original_configs:
                settings = config.find('settings')
                if settings is not None:
                    display = settings.find('display')

                    # Check if this is a separator entry
                    active = config.find('active')
                    if active is not None and active.text == 'false':
                        stats['separators'] += 1
                        continue

                    # Duplicate for UPPER device
                    if display is not None and display.get('serial') == original_serial:
                        upper_config = duplicate_config(config, original_serial, UPPER_SERIAL)
                        outputs.append(upper_config)
                        stats['outputs_upper'] += 1

                        # Duplicate for LOWER device
                        lower_config = duplicate_config(config, original_serial, LOWER_SERIAL)
                        outputs.append(lower_config)
                        stats['outputs_lower'] += 1

    # Process inputs section - clear all existing entries first
    inputs = root.find('inputs')
    if inputs is not None:
        # Remove all existing configs
        for config in list(inputs.findall('config')):
            inputs.remove(config)

        # Get fresh inputs from ROOT CONFIG
        root_inputs = root_root.find('inputs')

        if root_inputs is not None:
            original_configs = list(root_inputs.findall('config'))
            stats['inputs_original'] = len(original_configs)

            for config in original_configs:
                settings = config.find('settings')
                if settings is not None:
                    # Skip inactive entries
                    active = config.find('active')
                    if active is not None and active.text == 'false':
                        continue

                    # Duplicate for UPPER device
                    if settings.get('serial') == original_serial:
                        upper_config = duplicate_config(config, original_serial, UPPER_SERIAL)
                        inputs.append(upper_config)
                        stats['inputs_upper'] += 1

                        # Duplicate for LOWER device
                        lower_config = duplicate_config(config, original_serial, LOWER_SERIAL)
                        inputs.append(lower_config)
                        stats['inputs_lower'] += 1

    # Write the output file
    print(f"Writing combined configuration to: {OUTPUT_FILE}")
    tree.write(OUTPUT_FILE, encoding='UTF-8', xml_declaration=True)

    # Print statistics
    print()
    print("=" * 70)
    print("Configuration created successfully!")
    print("=" * 70)
    print()
    print(f"Statistics:")
    print(f"  Original outputs:     {stats['outputs_original']} (including {stats['separators']} separators)")
    print(f"  Duplicated for UPPER: {stats['outputs_upper']} outputs + {stats['inputs_upper']} inputs")
    print(f"  Duplicated for LOWER: {stats['outputs_lower']} outputs + {stats['inputs_lower']} inputs")
    print(f"  Total configs:        {stats['outputs_original'] + stats['outputs_upper'] + stats['outputs_lower'] + stats['inputs_original'] + stats['inputs_upper'] + stats['inputs_lower']}")
    print()
    print(f"Next steps:")
    print(f"  1. Open MobiFlight Connector")
    print(f"  2. Load the file: {OUTPUT_FILE.name}")
    print(f"  3. Verify both UPPER and LOWER devices are recognized")
    print(f"  4. Test your dual G5 setup!")
    print()


def main():
    """Main entry point for the script."""
    try:
        create_dual_g5_config()
    except Exception as e:
        print()
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()

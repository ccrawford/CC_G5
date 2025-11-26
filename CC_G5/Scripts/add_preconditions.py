#!/usr/bin/env python3
"""
Add Device Preconditions to MobiFlight Configuration

This script adds preconditions to CustomDevice outputs based on messageType ranges:
- messageType 0-29:  Common messages (no precondition added)
- messageType 30-59: HSI-specific (ccDeviceID = 0)
- messageType 60-99: PFD-specific (ccDeviceID = 1)

Usage:
    python add_preconditions.py <input.mcc> <output.mcc>

Example:
    python add_preconditions.py "G5 ROOT CONFIG.mcc" "G5 ROOT TUNED.mcc"
"""

import sys
import xml.etree.ElementTree as ET
from pathlib import Path


# Register the namespace to preserve the msdata prefix
ET.register_namespace('msdata', 'urn:schemas-microsoft-com:xml-msdata')


def add_preconditions(input_file, output_file):
    """
    Add preconditions to CustomDevice outputs based on messageType.

    Args:
        input_file: Path to input .mcc file
        output_file: Path to output .mcc file
    """
    try:
        tree = ET.parse(input_file)
        root = tree.getroot()
    except ET.ParseError as e:
        print(f"Error parsing XML file: {e}", file=sys.stderr)
        return 1
    except FileNotFoundError:
        print(f"Error: File not found: {input_file}", file=sys.stderr)
        return 1

    configs = root.findall('.//outputs/config')

    if not configs:
        print(f"No output configurations found in {input_file}", file=sys.stderr)
        return 1

    modified_count = 0
    skipped_count = 0

    for config in configs:
        # Find CustomDevice display elements
        display = config.find('.//display[@type="CustomDevice"]')

        if display is not None:
            message_type_str = display.get('messageType', '')

            if message_type_str:
                try:
                    message_type = int(message_type_str)
                except ValueError:
                    print(f"Warning: Invalid messageType '{message_type_str}', skipping", file=sys.stderr)
                    continue

                # Find or create preconditions element
                preconditions = config.find('.//preconditions')

                if preconditions is None:
                    # Create preconditions element if it doesn't exist
                    settings = config.find('.//settings')
                    if settings is not None:
                        preconditions = ET.SubElement(settings, 'preconditions')

                # Determine if precondition should be added
                device_id = None
                action = "skipped (common)"

                if 30 <= message_type <= 59:
                    # HSI-specific
                    device_id = "0"
                    action = "added HSI precondition (ccDeviceID=0)"
                elif 60 <= message_type <= 99:
                    # PFD-specific
                    device_id = "1"
                    action = "added PFD precondition (ccDeviceID=1)"

                if device_id is not None:
                    # Check if precondition already exists
                    existing = preconditions.find('.//precondition[@ref="ccDeviceID"]')

                    if existing is None:
                        # Add new precondition
                        precondition = ET.SubElement(preconditions, 'precondition')
                        precondition.set('type', 'variable')
                        precondition.set('active', 'true')
                        precondition.set('ref', 'ccDeviceID')
                        precondition.set('operand', '=')
                        precondition.set('value', device_id)
                        precondition.set('logic', 'and')
                        modified_count += 1
                    else:
                        # Update existing precondition
                        existing.set('value', device_id)
                        action = f"updated precondition (ccDeviceID={device_id})"
                        modified_count += 1
                else:
                    skipped_count += 1

                # Get description for reporting
                desc_elem = config.find('.//description')
                description = desc_elem.text if desc_elem is not None else "Unknown"

                print(f"MessageType {message_type:>3}: {description:<40} - {action}")

    # Write output file
    try:
        # Pretty print with indentation
        ET.indent(tree, space="  ")
        tree.write(output_file, encoding='UTF-8', xml_declaration=True)
        print(f"\nSuccess! Modified {modified_count} entries, skipped {skipped_count} common entries")
        print(f"Output written to: {output_file}")
        return 0
    except Exception as e:
        print(f"Error writing output file: {e}", file=sys.stderr)
        return 1


def main():
    """Main entry point."""
    if len(sys.argv) != 3:
        print(__doc__)
        return 1

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    return add_preconditions(input_file, output_file)


if __name__ == "__main__":
    sys.exit(main())

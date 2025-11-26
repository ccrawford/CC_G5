#!/usr/bin/env python3
"""
MobiFlight Configuration Output Report

This script parses a MobiFlight .mcc configuration file and generates a report
of all CustomDevice outputs, showing the object number, description, SimVar source,
and messageType.

Usage:
    python report_mcc_outputs.py <config_file.mcc>

Example:
    python report_mcc_outputs.py "G5 ROOT CONFIG.mcc"
    python report_mcc_outputs.py "../MF/G5 HSI.mcc"
"""

import sys
import xml.etree.ElementTree as ET
import re
from pathlib import Path


def generate_report(mcc_file):
    """
    Generate a formatted report of CustomDevice outputs from an MCC file.

    Args:
        mcc_file: Path to the .mcc configuration file
    """
    try:
        tree = ET.parse(mcc_file)
        root = tree.getroot()
    except ET.ParseError as e:
        print(f"Error parsing XML file: {e}", file=sys.stderr)
        return 1
    except FileNotFoundError:
        print(f"Error: File not found: {mcc_file}", file=sys.stderr)
        return 1

    configs = root.findall('.//outputs/config')

    if not configs:
        print(f"No output configurations found in {mcc_file}", file=sys.stderr)
        return 1

    # Collect all entries with CustomDevice outputs
    entries = []
    for idx, config in enumerate(configs, 1):
        desc_elem = config.find('description')
        description = desc_elem.text if desc_elem is not None else ""

        display = config.find('.//display[@type="CustomDevice"]')

        if display is not None:
            message_type = display.get('messageType', '')
        else:
            message_type = ''

        # Get the source Value field
        source = config.find('.//source')
        if source is not None:
            source_value = source.get('Value', '')
            # Remove all CR/LF and extra whitespace
            source_value = re.sub(r'\s+', ' ', source_value).strip()
        else:
            source_value = ''

        if message_type:
            # Truncate description to 40 chars
            if len(description) > 40:
                description = description[:37] + '...'

            # Truncate source value to 70 chars
            if len(source_value) > 70:
                source_value = source_value[:67] + '...'

            entries.append({
                'obj': idx,
                'desc': description,
                'value': source_value,
                'msg': int(message_type)
            })

    if not entries:
        print(f"No CustomDevice outputs found in {mcc_file}", file=sys.stderr)
        return 1

    # Sort by object number
    entries.sort(key=lambda x: x['obj'])

    # Print report header
    print(f"\nMobiFlight CustomDevice Output Report")
    print(f"File: {Path(mcc_file).name}")
    print(f"{'='*120}")
    print()

    # Print table header
    print(f"Obj#  {'Description':<40}  {'SimVar/Value':<70}  Msg")
    print(f"----  {'-'*40}  {'-'*70}  ---")

    # Print rows
    for e in entries:
        print(f"{e['obj']:>4}  {e['desc']:<40}  {e['value']:<70}  {e['msg']:>3}")

    print()
    print(f"Total: {len(entries)} CustomDevice outputs")
    print()

    return 0


def main():
    """Main entry point."""
    if len(sys.argv) != 2:
        print(__doc__)
        return 1

    mcc_file = sys.argv[1]

    return generate_report(mcc_file)


if __name__ == "__main__":
    sys.exit(main())

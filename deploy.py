import shutil
import os
import re
from pathlib import Path

def find_custom_source_folder():
    """Find the custom_source_folder value from platformio.ini files"""
    # Script is in project root directory
    project_root = Path(__file__).parent

    # Search for *_platformio.ini files in subdirectories (e.g., CC_G5/CC_G5_platformio.ini)
    ini_files = list(project_root.glob("*/*_platformio.ini"))

    if not ini_files:
        raise FileNotFoundError("No *_platformio.ini file found in project")

    # Parse the first one found
    ini_file = ini_files[0]
    print(f"Found platformio.ini: {ini_file}")

    with open(ini_file, 'r') as f:
        content = f.read()
        # Look for custom_source_folder = CC_G5
        match = re.search(r'custom_source_folder\s*=\s*(\S+)', content)
        if match:
            return match.group(1)

    raise ValueError("custom_source_folder not found in platformio.ini")

try:
    # Auto-discover the project folder name
    project_folder = find_custom_source_folder()
    print(f"Project folder: {project_folder}")

    # Get the project root directory (script is in project root)
    project_root = Path(__file__).parent
    source_dir = project_root / project_folder / "Community"

    print(f"Project root: {project_root}")
    print(f"Source directory: {source_dir}")
    print(f"Source exists: {source_dir.exists()}")

    if not source_dir.exists():
        raise FileNotFoundError(f"Community folder not found at {source_dir}")

    # Destination directory (using USERPROFILE environment variable)
    dest_dir = Path.home() / "AppData" / "Local" / "MobiFlight" / "MobiFlight Connector" / "Community" / project_folder

    print(f"Destination directory: {dest_dir}")

    # Create destination directory if it doesn't exist
    dest_dir.mkdir(parents=True, exist_ok=True)
    print(f"Destination created/verified")

    # Copy all contents from source to destination, overwriting existing files
    shutil.copytree(source_dir, dest_dir, dirs_exist_ok=True)

    print(f"Successfully copied contents from {source_dir} to {dest_dir}")

except Exception as e:
    print(f"Error: {e}")
    import traceback
    traceback.print_exc()

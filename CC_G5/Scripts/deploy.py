import shutil
import os
from pathlib import Path

try:
    # Get the script directory and navigate to source
    script_dir = Path(__file__).parent
    source_dir = script_dir.parent / "Community"

    print(f"Script directory: {script_dir}")
    print(f"Source directory: {source_dir}")
    print(f"Source exists: {source_dir.exists()}")

    # Destination directory
    dest_dir = Path(r"C:\Users\chris\AppData\Local\MobiFlight\MobiFlight Connector\Community\CC_G5")

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

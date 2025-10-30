import shutil
import os

src_folder = os.path.join(os.path.dirname(__file__), '..', 'Community')
dst_folder = r'C:\Users\chris\source\repos\FSHardware\CC_G5\CC_G5\Community'

# Ensure destination folder exists
os.makedirs(dst_folder, exist_ok=True)

for item in os.listdir(src_folder):
    src_path = os.path.join(src_folder, item)
    dst_path = os.path.join(dst_folder, item)
    if os.path.isfile(src_path):
        shutil.copy2(src_path, dst_path)
    elif os.path.isdir(src_path):
        # Copy entire directory, overwrite if exists
        if os.path.exists(dst_path):
            shutil.rmtree(dst_path)
        shutil.copytree(src_path, dst_path)
import subprocess
import sys
import os
import glob
import re
from pathlib import Path

preserve_none = ''
# Fix the path to be relative to the script location
script_dir = Path(__file__).parent
project_root = script_dir.parent
hex_dir_default = str(project_root / 'bootloader/node90_bootloader.X/dist/default/production/')
hex_file_default = 'node90_bootloader.X.production.unified.hex'

release_dir = str(project_root / 'release/')

def flash(hex_dir=hex_dir_default, hex_file=hex_file_default, preserve=preserve_none):        
    print("Flashing bootloader...")
    print("Hex dir: ", hex_dir)
    print("Hex file: ", hex_file)
    print("Preserve: ", preserve)

    # Construct the full path to the hex file
    full_hex_path = os.path.join(hex_dir, hex_file)
    
    # Check if the hex file exists
    if not os.path.exists(full_hex_path):
        print(f"Error: Hex file not found: {full_hex_path}")
        return False

    program_cmd = 'java -jar /opt/microchip/mplabx/v6.15/mplab_platform/mplab_ipe/ipecmd.jar -P32MX795F512H -F'+full_hex_path+' -MC -MB -MP1D000000,1D07FFFF'+preserve+' -TPICD4 -OL'
    split_program_cmd = program_cmd.split(' ');    
    result = subprocess.run(split_program_cmd)
    return result.returncode == 0

def get_latest_release_hex_file(release_dir=release_dir):
    """Find the latest release file in the release directory"""
    # release_dir is already an absolute path, so use it directly
    release_path = Path(release_dir)
    
    if not release_path.exists():
        print(f"Release directory not found: {release_path}")
        return None
    
    # Find all merged hex files
    pattern = "node90_*_merged_bootloader_*.hex"
    hex_files = list(release_path.glob(pattern))
    
    if not hex_files:
        print(f"No merged hex files found in {release_path}")
        return None
    
    # Extract versions and find the latest
    latest_file = None
    latest_version = None
    
    for hex_file in hex_files:
        # Parse filename: node90_xx.xx.xx_merged_bootloader_xx.xx.xx.hex
        match = re.match(r'node90_(\d+\.\d+\.\d+)_merged_bootloader_\d+\.\d+\.\d+\.hex', hex_file.name)
        if match:
            version = match.group(1)
            if latest_version is None or _compare_versions(version, latest_version) > 0:
                latest_version = version
                latest_file = hex_file
    
    if latest_file:
        print(f"Latest release found: {latest_file.name}")
        return str(latest_file)
    else:
        print("No valid merged hex files found")
        return None

def get_version_release_hex_file(version, release_dir=release_dir):
    """Find the release file in the release directory by version"""
    # release_dir is already an absolute path, so use it directly
    release_path = Path(release_dir)
    
    if not release_path.exists():
        print(f"Release directory not found: {release_path}")
        return None
    
    # Look for files matching the version pattern
    pattern = f"node90_{version}_merged_bootloader_*.hex"
    hex_files = list(release_path.glob(pattern))
    
    if not hex_files:
        print(f"No merged hex files found for version {version} in {release_path}")
        return None
    
    if len(hex_files) > 1:
        print(f"Multiple files found for version {version}:")
        for f in hex_files:
            print(f"  - {f.name}")
        print("Using the first one found")
    
    hex_file = hex_files[0]
    print(f"Found release file: {hex_file.name}")
    return str(hex_file)

def _compare_versions(version1, version2):
    """Compare two version strings. Returns 1 if version1 > version2, -1 if version1 < version2, 0 if equal"""
    v1_parts = [int(x) for x in version1.split('.')]
    v2_parts = [int(x) for x in version2.split('.')]
    
    for i in range(max(len(v1_parts), len(v2_parts))):
        v1_part = v1_parts[i] if i < len(v1_parts) else 0
        v2_part = v2_parts[i] if i < len(v2_parts) else 0
        
        if v1_part > v2_part:
            return 1
        elif v1_part < v2_part:
            return -1
    
    return 0

def main():
    if len(sys.argv) == 1:
        # No arguments - flash default hex
        print("Flashing default hex file...")
        flash()
    elif len(sys.argv) == 2:
        arg = sys.argv[1]
        if arg == "latest":
            # Flash latest release
            print("Finding latest release...")
            hex_file = get_latest_release_hex_file()
            if hex_file:
                # Extract directory and filename
                hex_path = Path(hex_file)
                hex_dir = str(hex_path.parent) + '/'
                hex_filename = hex_path.name
                flash(hex_dir, hex_filename)
            else:
                print("Failed to find latest release")
                sys.exit(1)
        else:
            # Assume it's a version
            print(f"Finding release for version {arg}...")
            hex_file = get_version_release_hex_file(arg)
            if hex_file:
                # Extract directory and filename
                hex_path = Path(hex_file)
                hex_dir = str(hex_path.parent) + '/'
                hex_filename = hex_path.name
                flash(hex_dir, hex_filename)
            else:
                print(f"Failed to find release for version {arg}")
                sys.exit(1)
    else:
        print("Usage:")
        print("  python flash.py                    # Flash default hex")
        print("  python flash.py latest             # Flash latest release")
        print("  python flash.py <version>          # Flash specific version")
        print("\nExamples:")
        print("  python flash.py")
        print("  python flash.py latest")
        print("  python flash.py 1.0.1")
        sys.exit(1)

if __name__ == "__main__":
    main()
#!/usr/bin/env python3
import subprocess
import sys
import os
import re
import shutil
from pathlib import Path

try:
    import intelhex
except ImportError:
    print("Error: intelhex module not found. Install with: pip install intelhex")
    sys.exit(1)

class ReleaseManager:
    def __init__(self):
        self.app_version_file = "src/project_version.h"
        self.bootloader_version_file = "bootloader/src/project_version.h"
        self.release_dir = "release"
        
    def parse_version(self, version_str):
        """Parse version string into major, minor, patch"""
        match = re.match(r'(\d+)\.(\d+)\.(\d+)', version_str)
        if not match:
            raise ValueError(f"Invalid version format: {version_str}")
        return int(match.group(1)), int(match.group(2)), int(match.group(3))
    
    def bump_version(self, current_version, bump_type):
        """Bump version based on type"""
        major, minor, patch = self.parse_version(current_version)
        
        if bump_type == "major":
            return f"{major + 1}.0.0"
        elif bump_type == "minor":
            return f"{major}.{minor + 1}.0"
        elif bump_type == "patch":
            return f"{major}.{minor}.{patch + 1}"
        else:
            raise ValueError(f"Invalid bump type: {bump_type}")
    
    def update_version_file(self, file_path, new_version):
        """Update version in project_version.h file"""
        with open(file_path, 'r') as f:
            content = f.read()
        
        # Replace PROJECT_VERSION line
        content = re.sub(
            r'#define PROJECT_VERSION "([^"]*)"',
            f'#define PROJECT_VERSION "{new_version}"',
            content
        )
        
        with open(file_path, 'w') as f:
            f.write(content)
        
        print(f"Updated {file_path} to version {new_version}")
    
    def get_current_version(self, file_path):
        """Get current version from project_version.h file"""
        with open(file_path, 'r') as f:
            content = f.read()
        
        match = re.search(r'#define PROJECT_VERSION "([^"]*)"', content)
        if not match:
            raise ValueError(f"Could not find PROJECT_VERSION in {file_path}")
        
        return match.group(1)
    
    def build_application(self):
        """Build application project"""
        print("Building application...")
        try:
            subprocess.run(["make", "-f", "nbproject/Makefile-default.mk", "SUBPROJECTS=", ".build-conf"], 
                         cwd="node90.X", check=True, capture_output=True, text=True)
            print("✓ Application built successfully")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ Failed to build application: {e}")
            print(f"Error output: {e.stderr}")
            return False
    
    def build_bootloader(self):
        """Build bootloader project"""
        print("Building bootloader...")
        try:
            subprocess.run(["make", "-f", "nbproject/Makefile-default.mk", "SUBPROJECTS=", ".build-conf"], 
                         cwd="bootloader/node90_bootloader.X", check=True, capture_output=True, text=True)
            print("✓ Bootloader built successfully")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ Failed to build bootloader: {e}")
            print(f"Error output: {e.stderr}")
            return False
    
    def hex_to_bin(self, hex_file, bin_file):
        """Convert hex file to binary"""
        print(f"Converting {hex_file} to binary...")
        ih = intelhex.IntelHex(hex_file)
        
        # Define the address range for application (0x9D004000 - 0x9D07FFFF)
        start = 0x9D004000
        end = 0x9D07FFFF
        size = end - start + 1
        
        # Extract binary data for the application range
        binary_data = ih.tobinarray(start=start, size=size)
        
        # Write binary file
        with open(bin_file, 'wb') as f:
            f.write(binary_data)
        
        print(f"✓ Binary created: {bin_file}")
    
    def create_release(self, bump_type="patch"):
        """Create a release"""
        
        # Get current application version and bump it
        current_version = self.get_current_version(self.app_version_file)
        new_app_version = self.bump_version(current_version, bump_type)
        self.update_version_file(self.app_version_file, new_app_version)
        
        # Get current bootloader version (for file naming)
        bootloader_version = self.get_current_version(self.bootloader_version_file)
        
        # Build application
        if not self.build_application():
            print("Application build failed. Aborting release.")
            return False
        
        # Build bootloader
        if not self.build_bootloader():
            print("Bootloader build failed. Aborting release.")
            return False
        
        # Create release directory
        os.makedirs(self.release_dir, exist_ok=True)
        
        # Copy and rename files
        release_files = []
        
        # Application binary (converted from hex)
        app_hex_src = "node90.X/dist/default/production/node90.X.production.hex"
        app_bin_dst = f"{self.release_dir}/node90_{new_app_version}.bin"
        if os.path.exists(app_hex_src):
            self.hex_to_bin(app_hex_src, app_bin_dst)
            release_files.append(app_bin_dst)
            print(f"✓ Application binary: {app_bin_dst}")
        else:
            print(f"✗ Application hex file not found: {app_hex_src}")
            return False
        
        # Unified hex from bootloader project
        unified_hex_src = "bootloader/node90_bootloader.X/dist/default/production/node90_bootloader.X.production.unified.hex"
        unified_hex_dst = f"{self.release_dir}/node90_{new_app_version}_merged_bootloader_{bootloader_version}.hex"
        if os.path.exists(unified_hex_src):
            shutil.copy2(unified_hex_src, unified_hex_dst)
            release_files.append(unified_hex_dst)
            print(f"✓ Unified hex: {unified_hex_dst}")
        else:
            print(f"✗ Unified hex file not found: {unified_hex_src}")
            return False
        
        print(f"\n🎉 Release {new_app_version} created successfully!")
        print(f"Release files in {self.release_dir}/:")
        for file in release_files:
            print(f"  - {os.path.basename(file)}")
        
        return True

def main():
    if len(sys.argv) != 2:
        print("Usage:")
        print("  python create_release.py [patch|minor|major]")
        print("\nExamples:")
        print("  python create_release.py patch")
        print("  python create_release.py minor")
        print("  python create_release.py major")
        sys.exit(1)
    
    release_mgr = ReleaseManager()
    bump_type = sys.argv[1]
    
    # Validate bump type
    valid_bumps = ["patch", "minor", "major"]
    
    if bump_type not in valid_bumps:
        print(f"Invalid bump type: {bump_type}")
        print(f"Valid bump types: {', '.join(valid_bumps)}")
        sys.exit(1)
    
    # Create release
    success = release_mgr.create_release(bump_type)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()

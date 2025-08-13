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
    
    def build_project(self, project_path):
        """Build a project"""
        print(f"Building {project_path}...")
        try:
            subprocess.run(["make", "-C", project_path, "production"], 
                         check=True, capture_output=True, text=True)
            print(f"✓ {project_path} built successfully")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ Failed to build {project_path}: {e}")
            print(f"Error output: {e.stderr}")
            return False
    
    def merge_hex_files(self, bootloader_hex, app_hex, merged_hex):
        """Merge bootloader and application hex files"""
        print("Creating merged hex file...")
        
        # Read hex files
        bootloader_ih = intelhex.IntelHex(bootloader_hex)
        app_ih = intelhex.IntelHex(app_hex)
        
        # Create merged hex
        merged_ih = intelhex.IntelHex()
        
        # Add bootloader data (0x9D000000 - 0x9D00FFFF)
        for addr in bootloader_ih.addresses():
            if 0x9D000000 <= addr <= 0x9D00FFFF:
                merged_ih[addr] = bootloader_ih[addr]
        
        # Add application data (0x9D010000 - 0x9D07FFFF)
        for addr in app_ih.addresses():
            if 0x9D010000 <= addr <= 0x9D07FFFF:
                merged_ih[addr] = app_ih[addr]
        
        # Write merged hex
        merged_ih.write_hex_file(merged_hex)
        print(f"✓ Merged hex created: {merged_hex}")
    
    def hex_to_bin(self, hex_file, bin_file):
        """Convert hex file to binary"""
        print(f"Converting {hex_file} to binary...")
        ih = intelhex.IntelHex(hex_file)
        ih.write_bin_file(bin_file)
        print(f"✓ Binary created: {bin_file}")
    
    def create_release(self, target="both", bump_type="patch"):
        """Create a release"""
        
        # Determine which version files to update
        if target == "bootloader":
            version_files = [self.bootloader_version_file]
            projects_to_build = ["bootloader/node90_bootloader.X"]
        elif target == "application":
            version_files = [self.app_version_file]
            projects_to_build = ["node90.X"]
        else:  # both
            version_files = [self.app_version_file, self.bootloader_version_file]
            projects_to_build = ["bootloader/node90_bootloader.X", "node90.X"]
        
        # Get current versions and bump them
        versions = {}
        for version_file in version_files:
            current_version = self.get_current_version(version_file)
            new_version = self.bump_version(current_version, bump_type)
            versions[version_file] = new_version
            self.update_version_file(version_file, new_version)
        
        # Build projects
        for project in projects_to_build:
            if not self.build_project(project):
                print("Build failed. Aborting release.")
                return False
        
        # Create release directory
        os.makedirs(self.release_dir, exist_ok=True)
        
        # Get application version for file naming
        app_version = versions.get(self.app_version_file, 
                                 self.get_current_version(self.app_version_file))
        
        # Copy and rename files
        release_files = []
        
        # Bootloader hex
        bootloader_src = "bootloader/node90_bootloader.X/dist/default/production/node90_bootloader.X.production.hex"
        bootloader_dst = f"{self.release_dir}/node90_bootloader_{app_version}.hex"
        if os.path.exists(bootloader_src):
            shutil.copy2(bootloader_src, bootloader_dst)
            release_files.append(bootloader_dst)
            print(f"✓ Bootloader hex: {bootloader_dst}")
        
        # Application hex
        app_src = "node90.X/dist/default/production/node90.X.production.hex"
        app_dst = f"{self.release_dir}/node90_{app_version}.hex"
        if os.path.exists(app_src):
            shutil.copy2(app_src, app_dst)
            release_files.append(app_dst)
            print(f"✓ Application hex: {app_dst}")
            
            # Convert to binary
            bin_dst = f"{self.release_dir}/node90_{app_version}.bin"
            self.hex_to_bin(app_dst, bin_dst)
            release_files.append(bin_dst)
        
        # Merged hex
        merged_dst = f"{self.release_dir}/node90_merged_{app_version}.hex"
        if os.path.exists(app_src) and os.path.exists(bootloader_src):
            self.merge_hex_files(bootloader_src, app_src, merged_dst)
            release_files.append(merged_dst)
        
        print(f"\n🎉 Release {app_version} created successfully!")
        print(f"Release files in {self.release_dir}/:")
        for file in release_files:
            print(f"  - {os.path.basename(file)}")
        
        return True

def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python create_release.py [patch|minor|major]")
        print("  python create_release.py [bootloader|application|both] [patch|minor|major]")
        print("\nExamples:")
        print("  python create_release.py patch")
        print("  python create_release.py bootloader minor")
        print("  python create_release.py application major")
        sys.exit(1)
    
    release_mgr = ReleaseManager()
    
    if len(sys.argv) == 2:
        # Only bump type specified - default to both
        bump_type = sys.argv[1]
        target = "both"
    else:
        # Target and bump type specified
        target = sys.argv[1]
        bump_type = sys.argv[2]
    
    # Validate arguments
    valid_targets = ["bootloader", "application", "both"]
    valid_bumps = ["patch", "minor", "major"]
    
    if target not in valid_targets:
        print(f"Invalid target: {target}")
        print(f"Valid targets: {', '.join(valid_targets)}")
        sys.exit(1)
    
    if bump_type not in valid_bumps:
        print(f"Invalid bump type: {bump_type}")
        print(f"Valid bump types: {', '.join(valid_bumps)}")
        sys.exit(1)
    
    # Create release
    success = release_mgr.create_release(target, bump_type)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()

# tools/build_all.py
#!/usr/bin/env python3
import subprocess
import os
import shutil

class BuildManager:
    def __init__(self):
        self.bootloader_dir = "bootloader/node90_bootloader.X"
        self.app_dir = "application/node90.X"
        self.merged_dir = "merged/node90_merged.X"
        
    def build_bootloader(self):
        """Build only bootloader"""
        print("Building bootloader...")
        subprocess.run(["make", "-C", self.bootloader_dir, "production"], check=True)
        print("Bootloader built successfully!")
        
    def build_application(self):
        """Build only application"""
        print("Building application...")
        subprocess.run(["make", "-C", self.app_dir, "production"], check=True)
        print("Application built successfully!")
        
    def build_merged(self):
        """Build merged firmware"""
        print("Building merged firmware...")
        # First build both components
        self.build_bootloader()
        self.build_application()
        
        # Then merge them
        self.merge_hex_files()
        print("Merged firmware built successfully!")
        
    def merge_hex_files(self):
        """Merge bootloader and application hex files"""
        bootloader_hex = f"{self.bootloader_dir}/dist/default/production/node90_bootloader.X.production.hex"
        app_hex = f"{self.app_dir}/dist/default/production/node90.X.production.hex"
        merged_hex = f"{self.merged_dir}/dist/default/production/node90_merged.X.production.hex"
        
        # Create merged hex file
        with open(merged_hex, 'w') as outfile:
            # Write bootloader section (0x9D000000 - 0x9D00FFFF)
            with open(bootloader_hex, 'r') as bootfile:
                for line in bootfile:
                    if line.startswith(':'):
                        # Parse Intel HEX format and filter bootloader addresses
                        if self.is_bootloader_address(line):
                            outfile.write(line)
            
            # Write application section (0x9D010000 - 0x9D07FFFF)
            with open(app_hex, 'r') as appfile:
                for line in appfile:
                    if line.startswith(':'):
                        # Parse Intel HEX format and filter application addresses
                        if self.is_application_address(line):
                            outfile.write(line)
            
            # Write EOF record
            outfile.write(":00000001FF\n")
    
    def is_bootloader_address(self, hex_line):
        """Check if hex line is in bootloader address range"""
        # Parse Intel HEX address
        if len(hex_line) < 9:
            return False
        addr_high = int(hex_line[3:5], 16)
        addr_low = int(hex_line[5:7], 16)
        addr = (addr_high << 8) | addr_low
        
        # Check if in bootloader range (0x9D000000 - 0x9D00FFFF)
        return 0x0000 <= addr <= 0xFFFF
    
    def is_application_address(self, hex_line):
        """Check if hex line is in application address range"""
        # Parse Intel HEX address
        if len(hex_line) < 9:
            return False
        addr_high = int(hex_line[3:5], 16)
        addr_low = int(hex_line[5:7], 16)
        addr = (addr_high << 8) | addr_low
        
        # Check if in application range (0x9D010000 - 0x9D07FFFF)
        return 0x0100 <= addr <= 0x07FF

if __name__ == "__main__":
    import sys
    
    build_mgr = BuildManager()
    
    if len(sys.argv) < 2:
        print("Usage: python build_all.py [bootloader|application|merged]")
        sys.exit(1)
        
    target = sys.argv[1]
    
    if target == "bootloader":
        build_mgr.build_bootloader()
    elif target == "application":
        build_mgr.build_application()
    elif target == "merged":
        build_mgr.build_merged()
    else:
        print("Invalid target. Use: bootloader, application, or merged")
        sys.exit(1)

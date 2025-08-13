# tools/flash.py
#!/usr/bin/env python3
import subprocess
import sys
import os
import glob

class FlashManager:
    def __init__(self):
        self.device = "PIC32MX795F512H"
        self.programmer = "ICD4"  # or "PK3", "PICkit4", etc.
        self.release_dir = "release"
        
    def flash_bootloader(self, version=None):
        """Flash bootloader"""
        if version:
            hex_file = f"{self.release_dir}/node90_bootloader_{version}.hex"
        else:
            hex_file = "bootloader/node90_bootloader.X/dist/default/production/node90_bootloader.X.production.hex"
        self._flash_hex(hex_file, "0x9D000000-0x9D00FFFF")
        
    def flash_application(self, version=None):
        """Flash application"""
        if version:
            hex_file = f"{self.release_dir}/node90_{version}.hex"
        else:
            hex_file = "node90.X/dist/default/production/node90.X.production.hex"
        self._flash_hex(hex_file, "0x9D010000-0x9D07FFFF")
        
    def flash_merged(self, version=None):
        """Flash merged firmware"""
        if version:
            hex_file = f"{self.release_dir}/node90_merged_{version}.hex"
        else:
            hex_file = "merged/node90_merged.X/dist/default/production/node90_merged.X.production.hex"
        self._flash_hex(hex_file, "0x9D000000-0x9D07FFFF")
        
    def _flash_hex(self, hex_file, address_range):
        """Flash hex file to specific address range"""
        if not os.path.exists(hex_file):
            print(f"Error: Hex file not found: {hex_file}")
            sys.exit(1)
            
        # Using MPLAB X IPE command line
        # cmd = [
        #     "ipe.sh",  # or "ipe.bat" on Windows
        #     "-T", self.device,
        #     "-P", self.programmer,
        #     "-F", hex_file,
        #     "-M",  # Program memory
        #     "-R", address_range  # Address range
        # ]

        preserve_NVM = ' -OP1D07F000-1D07FFFF'
        preserve_none = ''
        preserve = preserve_none

        program_cmd = 'java -jar /opt/microchip/mplabx/v6.25/mplab_platform/mplab_ipe/ipecmd.jar -P32MX795F512H -F'+hex_file+' -MC -MB -MP1D000000,1D07FFFF'+preserve+' -TPICD4 -OL'
        split_program_cmd = program_cmd.split(' ');
        
        print(f"Flashing {hex_file} to {address_range}...")
        subprocess.run(split_program_cmd, check=True)
        print("Flash completed successfully!")
    
    def list_releases(self):
        """List available releases"""
        if not os.path.exists(self.release_dir):
            print("No releases found.")
            return
        
        print("Available releases:")
        hex_files = glob.glob(f"{self.release_dir}/*.hex")
        versions = set()
        
        for hex_file in hex_files:
            filename = os.path.basename(hex_file)
            # Extract version from filename
            if filename.startswith("node90_") and filename.endswith(".hex"):
                version = filename[6:-4]  # Remove "node90_" prefix and ".hex" suffix
                if "_" in version:
                    version = version.split("_", 1)[1]  # Get part after underscore
                versions.add(version)
        
        for version in sorted(versions):
            print(f"  {version}")

def main():
    flash_mgr = FlashManager()
    
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python flash.py [bootloader|application|merged] [version]")
        print("  python flash.py list")
        print("\nExamples:")
        print("  python flash.py bootloader")
        print("  python flash.py application 1.2.3")
        print("  python flash.py merged release 1.2.3")
        print("  python flash.py list")
        sys.exit(1)
    
    target = sys.argv[1]
    
    if target == "list":
        flash_mgr.list_releases()
        return
    
    if target not in ["bootloader", "application", "merged"]:
        print("Invalid target. Use: bootloader, application, merged, or list")
        sys.exit(1)
    
    # Check for version argument
    version = None
    if len(sys.argv) >= 3:
        if sys.argv[2] == "release" and len(sys.argv) >= 4:
            version = sys.argv[3]
        else:
            version = sys.argv[2]
    
    # Flash the target
    if target == "bootloader":
        flash_mgr.flash_bootloader(version)
    elif target == "application":
        flash_mgr.flash_application(version)
    elif target == "merged":
        flash_mgr.flash_merged(version)

if __name__ == "__main__":
    main()

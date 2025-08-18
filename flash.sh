#!/bin/bash

# Simple flash script that calls flash.py
# Usage: ./flash.sh [latest|<version>]

if [ $# -gt 1 ]; then
    echo "Usage: $0 [latest|<version>]"
    echo ""
    echo "Examples:"
    echo "  $0                    # Flash default hex"
    echo "  $0 latest             # Flash latest release"
    echo "  $0 1.0.1              # Flash specific version"
    exit 1
fi

# Run the Python flash script
if [ $# -eq 0 ]; then
    echo "Flashing default hex file..."
    python3 tools/flash.py
else
    ARG="$1"
    echo "Flashing with argument: $ARG"
    python3 tools/flash.py "$ARG"
fi

# Check exit status
if [ $? -eq 0 ]; then
    echo "Flash completed successfully!"
else
    echo "Flash failed!"
    exit 1
fi

# remove MPLABXLog.xml
rm MPLABXLog.xml
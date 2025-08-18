#!/bin/bash

# Simple release script that calls create_release.py
# Usage: ./release.sh [patch|minor|major]

if [ $# -ne 1 ]; then
    echo "Usage: $0 [patch|minor|major]"
    echo ""
    echo "Examples:"
    echo "  $0 patch"
    echo "  $0 minor"
    echo "  $0 major"
    exit 1
fi

BUMP_TYPE="$1"

# Validate argument
case "$BUMP_TYPE" in
    patch|minor|major)
        ;;
    *)
        echo "Error: Invalid bump type '$BUMP_TYPE'"
        echo "Valid options: patch, minor, major"
        exit 1
        ;;
esac

# Run the Python release script
echo "Creating release with $BUMP_TYPE version bump..."
python3 tools/create_release.py "$BUMP_TYPE"

# Check exit status
if [ $? -eq 0 ]; then
    echo "Release completed successfully!"
else
    echo "Release failed!"
    exit 1
fi

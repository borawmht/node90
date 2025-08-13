.PHONY: help bootloader application merged flash-bootloader flash-application flash-merged release list-releases

help:
	@echo "Node90 Firmware Build System"
	@echo ""
	@echo "Build targets:"
	@echo "  bootloader      - Build bootloader only"
	@echo "  application     - Build application only"
	@echo "  merged          - Build merged firmware"
	@echo ""
	@echo "Flash targets:"
	@echo "  flash-bootloader - Build and flash bootloader"
	@echo "  flash-application - Build and flash application"
	@echo "  flash-merged    - Build and flash merged firmware"
	@echo ""
	@echo "Release targets:"
	@echo "  release-patch   - Create patch release (1.0.0 -> 1.0.1)"
	@echo "  release-minor   - Create minor release (1.0.0 -> 1.1.0)"
	@echo "  release-major   - Create major release (1.0.0 -> 2.0.0)"
	@echo "  list-releases   - List available releases"

bootloader:
	@echo "Building bootloader..."
	@make -C bootloader/node90_bootloader.X production

application:
	@echo "Building application..."
	@make -C node90.X production

merged: bootloader application
	@echo "Building merged firmware..."
	@python tools/build_all.py merged

flash-bootloader: bootloader
	@echo "Flashing bootloader..."
	@python tools/flash.py bootloader

flash-application: application
	@echo "Flashing application..."
	@python tools/flash.py application

flash-merged: merged
	@echo "Flashing merged firmware..."
	@python tools/flash.py merged

release-patch:
	@python tools/create_release.py patch

release-minor:
	@python tools/create_release.py minor

release-major:
	@python tools/create_release.py major

list-releases:
	@python tools/flash.py list

clean:
	@make -C bootloader/node90_bootloader.X clean
	@make -C node90.X clean
	@make -C merged/node90_merged.X clean 
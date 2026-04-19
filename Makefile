GDK ?= /opt/sgdk
BLASTEM ?= blastem
DOCKER_IMAGE ?= schiv-genesis-build

.PHONY: all release debug clean run docker-release docker-debug help

all: release

help:
	@echo "Targets:"
	@echo "  make release         Build release ROM with local SGDK"
	@echo "  make debug           Build debug ROM with local SGDK"
	@echo "  make run             Build release ROM and run in BlastEm"
	@echo "  make docker-release  Build release ROM using Docker toolchain"
	@echo "  make docker-debug    Build debug ROM using Docker toolchain"
	@echo "  make clean           Remove build output"

release:
	$(MAKE) -f $(GDK)/makefile.gen release

debug:
	$(MAKE) -f $(GDK)/makefile.gen debug DEBUG=1 

docker-release:
	docker run --rm -v $(CURDIR):/src $(DOCKER_IMAGE) make release

docker-debug:
	docker run --rm -v $(CURDIR):/src $(DOCKER_IMAGE) make debug

clean:
	rm -rf out


run: release
	$(BLASTEM) ./out/rom.bin



# OpenOrbis PS4 Makefile — SmartTube PS4 v2.9
TITLE := SmartTube PS4
VERSION := 2.9
TITLE_ID := STBP00001
CONTENT_ID := IV0000-STBP00001_00-SMARTTUBEPS42900
TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
BIN_DIR := $(TOOLCHAIN)/bin/linux
BUILD_DIR := build
# create-fself: the CI builds it into .tools/, the released toolchain ships one in bin/linux.
FSELF := $(firstword $(wildcard .tools/create-fself) $(wildcard $(TOOLCHAIN)/bin/linux/create-fself))
# Sce module blobs live in bin/data/modules in the released (and Docker) toolchain,
# in src/modules in a source checkout.
MODULE_SRC := $(firstword $(wildcard $(TOOLCHAIN)/bin/data/modules) $(wildcard $(TOOLCHAIN)/src/modules) $(wildcard $(TOOLCHAIN)/samples/hello_world/sce_module))
OBJ := $(BUILD_DIR)/main_ps4.o
ELF := $(BUILD_DIR)/smarttube_ps4.elf
CC := clang
LD := ld.lld
LIBS := -lc -lkernel -lSceUserService -lSceVideoOut -lScePad -lSceAudioOut -lSceNet
CFLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O2 -Wall -Wextra -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o
.PHONY: all clean prepare
all: $(CONTENT_ID).pkg
prepare:
	@test -n "$(OO_PS4_TOOLCHAIN)" || (echo "OO_PS4_TOOLCHAIN is not set"; exit 1)
	@mkdir -p $(BUILD_DIR) sce_sys/about sce_module
	@test -n "$(FSELF)" -a -x "$(FSELF)" || (echo "OpenOrbis create-fself tool not found (.tools/create-fself or $(TOOLCHAIN)/bin/linux/create-fself)"; exit 1)
	@test -n "$(MODULE_SRC)" || (echo "OpenOrbis Sce module blobs not found under $(TOOLCHAIN)"; exit 1)
	@test -f "$(MODULE_SRC)/libc.prx" || (echo "OpenOrbis libc.prx module not found"; exit 1)
	@test -f "$(MODULE_SRC)/libSceFios2.prx" || (echo "OpenOrbis libSceFios2.prx module not found"; exit 1)
	cp "$(MODULE_SRC)/libc.prx" sce_module/libc.prx
	cp "$(MODULE_SRC)/libSceFios2.prx" sce_module/libSceFios2.prx
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
$(OBJ): native/src/main_ps4.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(ELF): $(OBJ)
	$(LD) $^ -o $@ $(LDFLAGS)
eboot.bin: $(ELF)
	$(FSELF) -in=$< -out=$(BUILD_DIR)/smarttube_ps4.oelf --eboot "$@" --paid 0x3800000000000011
sce_module/libc.prx sce_module/libSceFios2.prx: | prepare
sce_sys/param.sfo: Makefile | prepare
	$(BIN_DIR)/PkgTool.Core sfo_new $@
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(BIN_DIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'
$(CONTENT_ID).pkg: eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_module/libc.prx sce_module/libSceFios2.prx pkg.gp4
	$(BIN_DIR)/PkgTool.Core pkg_build pkg.gp4 .
clean:
	rm -rf $(BUILD_DIR) eboot.bin $(CONTENT_ID).pkg pkg.gp4.tmp
	rm -f sce_sys/param.sfo sce_module/libc.prx sce_module/libSceFios2.prx

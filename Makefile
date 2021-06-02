# This file is for use with GNU Make only.
# Project details
NAME		:= 3DS-LVGL
DESCRIPTION	:= Example application of the use of LVGL on the 3DS
COMPANY		:= Deltabeard
AUTHOR		:= Mahyar Koshkouei
LICENSE_SPDX	:= BSD-0

VERSION_MAJOR := 0
VERSION_MINOR := 0
VERSION_MICRO := 1

# Default configurable build options
BUILD	:= DEBUG

# Build help text shown using `make help`.
define help_txt
$(NAME) - $(DESCRIPTION)

Available options and their descriptions when enabled:
  BUILD=$(BUILD)
    The type of build configuration to use.
    This is one of DEBUG, RELEASE, RELDEBUG and RELMINSIZE.
      DEBUG: All debugging symbols; No optimisation.
      RELEASE: No debugging symbols; Optimised for speed.
      RELDEBUG: All debugging symbols; Optimised for speed.
      RELMINSIZE: No debugging symbols; Optimised for size.

  PLATFORM=$(PLATFORM)
    Manualy specify target platform. If unset, an attempt is made to
    automatically determine this.
    Supported platforms:
      MSVC: For Windows NT platforms compiled with Visual Studio C++ Build Tools.
            Must be compiled within the "Native Tools Command Prompt for VS" shell.
      3DS: For Nintendo 3DS homebrew platform. (Not automatically set).
      UNIX: For all Unix-like platforms, including Linux, BSD, MacOS, and MSYS2.

  EXTRA_CFLAGS=$(EXTRA_CFLAGS)
    Extra CFLAGS to pass to C compiler.

  EXTRA_LDFLAGS=$(EXTRA_LDFLAGS)
    Extra LDFLAGS to pass to the C compiler.

Example: make BUILD=RELEASE EXTRA_CFLAGS="-march=native"

$(LICENSE)
endef

# Default platform is UNIX unless certain conditions are met.
PLATFORM := UNIX
BRCK := /

ifdef VSCMD_VER
	PLATFORM := MSVC
	BRCK := $(strip \ )
else ifeq ($(MSYSTEM),MSYS)
	ifdef DEVKITARM
		PLATFORM := 3DS
	endif
endif

ifeq ($(PLATFORM),MSVC)
	# Default compiler options for Microsoft Visual C++ (MSVC)
	CC	:= cl
	OBJEXT	:= obj
	RM	:= del
	CFLAGS	:= /nologo /utf-8 /W1 /Iinc /Iext\inc /FS /D_CRT_SECURE_NO_WARNINGS
	LDFLAGS := /link /SUBSYSTEM:CONSOLE SDL2main.lib SDL2.lib shell32.lib /LIBPATH:ext\lib_$(VSCMD_ARG_TGT_ARCH)
	EXE	:= $(NAME).exe
	TARGET_FOLDER := $(strip out\WINDOWS_$(VSCMD_ARG_TGT_ARCH)\ )

else ifeq ($(PLATFORM),3DS)
#include $(DEVKITARM)/3ds_rules
	APP_TITLE := $(NAME)
	APP_DESCRIPTION := $(DESCRIPTION)
	APP_AUTHOR := $(COMPANY)
	APP_ICON := $(DEVKITPRO)/libctru/default_icon.png

	export PATH := $(DEVKITPRO)/tools/bin:$(DEVKITPRO)/devkitARM/bin:$(PATH)
	PREFIX	:= arm-none-eabi-
	CC	:= $(PREFIX)gcc
	CXX	:= $(PREFIX)g++
	EXE	:= $(NAME).3dsx $(NAME).3ds $(NAME).cia
	ARCH	:= armv6k
	TARGET_FOLDER := out/$(PLATFORM)_$(ARCH)/
	OBJEXT	:= o
	CFLAGS	:= -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -D__3DS__	\
		-I$(DEVKITPRO)/libctru/include
	LDFLAGS	= -specs=3dsx.specs -L$(DEVKITPRO)/libctru/lib -lctru

	PRODUCT_CODE	:= CTR-P-LVGL
	UNIQUE_ID		:= 0xFF3CD
	CATEGORY		:= Application
	USE_ON_SD		:= true
	MEMORY_TYPE		:= Application
	SYSTEM_MODE		:= 64MB
	SYSTEM_MODE_EXT	:= Legacy
	CPU_SPEED		:= 268MHz
	ENABLE_L2_CACHE	:= false
	MAKEROM_FLAGS := -rsf ext/3ds_cia_template.rsf -target t -exefslogo		\
		-icon meta/icon.icn -banner meta/banner.bnr				\
		-major $(VERSION_MAJOR) -minor $(VERSION_MINOR) -micro $(VERSION_MICRO)	\
		-DAPP_TITLE="$(NAME)" -DAPP_PRODUCT_CODE="$(PRODUCT_CODE)"		\
		-DAPP_UNIQUE_ID="$(UNIQUE_ID)" -DAPP_SYSTEM_MODE="$(SYSTEM_MODE)"	\
		-DAPP_SYSTEM_MODE_EXT="$(SYSTEM_MODE_EXT)" -DAPP_CATEGORY="$(CATEGORY)"	\
		-DAPP_USE_ON_SD="$(USE_ON_SD)" -DAPP_MEMORY_TYPE="$(MEMORY_TYPE)"	\
		-DAPP_CPU_SPEED="$(CPU_SPEED)" -DAPP_ENABLE_L2_CACHE="$(ENABLE_L2_CACHE)" \
		-DAPP_VERSION_MAJOR="$(VERSION_MAJOR)"					\
		-logo "meta/logo.bcma.lz"

else ifeq ($(PLATFORM),UNIX)
	# Check that pkg-config is available
	CHECK	:= $(shell which pkg-config)
ifneq ($(.SHELLSTATUS),0)
	err	:= $(error Unable to locate 'pkg-config' application)
endif
	ARCH	:= $(shell uname -m)
	OS	:= $(shell uname -s)

	# Default compiler options for GCC and Clang
	CC	:= cc
	OBJEXT	:= o
	RM	:= rm -f
	CFLAGS	:= -Wall -Wextra -D_DEFAULT_SOURCE $(shell pkg-config sdl2 --cflags)
	LDFLAGS	:= $(shell pkg-config sdl2 --libs)
	EXE	:= $(NAME).elf
	TARGET_FOLDER := out/$(OS)_$(ARCH)/

else
	err := $(error Unsupported platform specified)
endif

# Options specific to Windows NT 32-bit platforms
ifeq ($(VSCMD_ARG_TGT_ARCH),x86)
	# Use SSE instructions (since Pentium III).
	CFLAGS += /arch:SSE

	# Add support for ReactOS and Windows XP.
	CFLAGS += /Fdvc141.pdb
endif

#
# No need to edit anything past this line.
#
LICENSE := (C) $(AUTHOR). $(LICENSE_SPDX).
GIT_VER := $(shell git describe --dirty --always --tags --long)

SRCS := $(wildcard src/*.c) $(wildcard src/**/*.c) $(wildcard inc/lvgl/src/**/*.c)

# If using del, use Windows style folder separator.
ifeq ($(RM),del)
	SRCS := $(subst /,$(BRCK),$(SRCS))
endif

OBJS += $(SRCS:.c=.$(OBJEXT))

MKDIR := $(shell mkdir $(TARGET_FOLDER))
TARGET	+= $(addprefix $(TARGET_FOLDER),$(EXE))

# Use a fallback git version string if build system does not have git.
ifeq ($(GIT_VER),)
	GIT_VER := LOCAL
endif

# Function to check if running within MSVC Native Tools Command Prompt.
ISTARGNT = $(if $(VSCMD_VER),$(1),$(2))

# Select default build type
ifndef BUILD
	BUILD := DEBUG
endif

# Apply build type settings
ifeq ($(BUILD),DEBUG)
	CFLAGS += $(call ISTARGNT,/Zi /MDd /RTC1 /sdl,-Og -g3)
	CFLAGS += -D__DEBUG__
else ifeq ($(BUILD),RELEASE)
	CFLAGS += $(call ISTARGNT,/MD /O2 /fp:fast /GL /GT /Ot /O2,-Ofast -flto)
	LDFLAGS += $(call ISTARGNT,/LTCG,)
else ifeq ($(BUILD),RELDEBUG)
	CFLAGS += $(call ISTARGNT,/MDd /O2 /fp:fast,-Ofast -g3 -flto)
	CFLAGS += -D__DEBUG__
else ifeq ($(BUILD),RELMINSIZE)
	CFLAGS += $(call ISTARGNT,/MD /O1 /fp:fast /GL /GT /Os,-Os -ffast-math -flto)
else
	err := $(error Unknown build configuration '$(BUILD)')
endif

# When compiling with MSVC, check if SDL2 has been expanded from prepared cab file.
ifeq ($(CC)$(wildcard SDL2.dll),cl)
    $(info Preparing SDL2 development libraries)
    EXPAND_CMD := expand -F:* -R .\ext\MSVC_DEPS.cab ext
    UNUSED := $(shell $(EXPAND_CMD))

    # Copy SDL2.DLL to output EXE directory.
    UNUSED := $(shell COPY ext\lib_$(VSCMD_ARG_TGT_ARCH)\*.dll $(TARGET_FOLDER))
endif

override CFLAGS += -Iinc -Iinc/lvgl -DBUILD=$(BUILD) $(EXTRA_CFLAGS)
override LDFLAGS += $(EXTRA_LDFLAGS)

# Don't remove intermediate object files.
.PRECIOUS: %.c %.o %.elf

all: $(TARGET)

# Unix rules
%.elf: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

# MSVC rules
%.exe: $(OBJS)
	$(CC) $(CFLAGS) /Fe$@ $^ $(LDFLAGS)

%.obj: %.c
	$(CC) $(CFLAGS) /Fo$@ /c /TC $^

%.res: %.rc
	rc /nologo /DCOMPANY="$(COMPANY)" /DDESCRIPTION="$(DESCRIPTION)" \
		/DLICENSE="$(LICENSE)" /DGIT_VER="$(GIT_VER)" \
		/DNAME="$(NAME)" $^

# Nintendo 3DS rules for use with devkitARM
%.3dsx: %.elf %.smdh
	3dsxtool $< $@ --smdh=$(word 2,$^)

%.3ds: %.elf meta/banner.bnr meta/icon.icn
	makerom -f cci -o $@ -elf $< -DAPP_ENCRYPTED=true $(MAKEROM_FLAGS)

%.cia: %.elf meta/banner.bnr meta/icon.icn
	makerom -f cia -o $@ -elf $< -DAPP_ENCRYPTED=false $(MAKEROM_FLAGS)

%.bnr: meta/banner.png meta/banner.wav
	bannertool makebanner --image $(word 1,$^) --audio $(word 2,$^) --output $@

%.icn: meta/icon.png
	bannertool makesmdh -s "$(NAME)" -l "$(NAME) - $(DESCRIPTION)" -p "$(COMPANY)" --icon $^ -o $@

%.smdh: meta/icon.png
	smdhtool --create "$(NAME)" "$(DESCRIPTION)" "$(COMPANY)" $^ $@

clean:
	$(RM) $(TARGET) $(RES) $(OBJS)
	$(RM) $(TARGET:.exe=.ilk)
	$(RM) $(TARGET:.exe=.pdb)
	$(RM) $(SRCS:.c=.d) $(SRCS:.c=.gcda)

help:
	@exit
	$(info $(help_txt))

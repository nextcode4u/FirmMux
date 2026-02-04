.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
APP_TITLE := FirmMux
APP_DESCRIPTION := Unified CTR/TWL front-end
APP_AUTHOR := FirmMux Team
include $(DEVKITARM)/3ds_rules

TARGET		:=	FirmMux
BUILD		:=	build
SOURCES		:=	source
DATA		:=	assets
INCLUDES	:=	include

ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-ffunction-sections \
			$(ARCH)

CFLAGS	+=	-D__3DS__
ifneq ($(strip $(BUILD_ID)),)
CFLAGS	+=	-DFIRMUX_BUILD_ID=\"$(BUILD_ID)\"
endif

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lcitro2d -lcitro3d -lctru -lm

LIBDIRS	:= $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SOURCES)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			-I$(CURDIR) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)
override CFLAGS += $(INCLUDE)
override CXXFLAGS += $(INCLUDE)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXFLAGS += --smdh=$(OUTPUT).smdh

.PHONY: clean all

all: $(BUILD) $(OUTPUT).3dsx $(OUTPUT).smdh

$(BUILD):
	@mkdir -p $@

$(OUTPUT).3dsx	:	$(OUTPUT).elf $(OUTPUT).smdh

$(OUTPUT).elf	:	$(OFILES)

else

DEPENDS	:=	$(OFILES:.o=.d)

-include $(DEPENDS)

endif

%.bin.o : %.bin
	@echo $(notdir $<)
	@$(bin2o)

%_bin.h : %.bin
	@echo $(notdir $<)
	@$(bin2o)

%.bcfnt.o : %.bcfnt
	@echo $(notdir $<)
	@$(bin2o)

%_bcfnt.h : %.bcfnt
	@echo $(notdir $<)
	@$(bin2o)

%.png.o : %.png
	@echo $(notdir $<)
	@$(bin2o)

%_png.h : %.png
	@echo $(notdir $<)
	@$(bin2o)

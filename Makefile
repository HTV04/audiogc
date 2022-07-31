#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

# Check LIBOGC_VER
ifeq ($(strip $(LIBOGC_VER)),)
export LIBOGC_VER	:=	libogc-mod
else ifneq ($(LIBOGC_VER),libogc)
ifneq ($(LIBOGC_VER),libogc2)
ifneq ($(LIBOGC_VER),libogc-mod)
$(error "LIBOGC_VER must be libogc-mod, libogc, libogc2")
endif
endif
endif

# Check PLATFORM and set up environment
ifeq ($(PLATFORM),gamecube)
ifeq ($(LIBOGC_VER),libogc)
include $(DEVKITPPC)/gamecube_rules
else
include $(DEVKITPRO)/$(LIBOGC_VER)/gamecube_rules
endif
else
ifeq ($(LIBOGC_VER),libogc)
include $(DEVKITPPC)/wii_rules
else
include $(DEVKITPRO)/$(LIBOGC_VER)/wii_rules
endif
endif

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	lib$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	src
DATA		:=	data
INCLUDES	:=	include

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

CFLAGS	=	-Wall $(MACHDEP) $(INCLUDE)
CFLAGS	+=	-DNDEBUG -O2
#CFLAGS	+= -DDEBUG -g -O0
ifeq ($(LIBOGC_VER),libogc)
CFLAGS	+=	-DAUDIOGC_LIBOGC_OG
else ifeq ($(LIBOGC_VER),libogc2)
CFLAGS	+=	-DAUDIOGC_LIBOGC_2
else
CFLAGS	+=	-DAUDIOGC_LIBOGC_MOD
endif
CXXFLAGS	=	$(CFLAGS)

LDFLAGS	=	$(MACHDEP) -Wl,-Map,$(notdir $@).map
#LDFLAGS	+=	-g

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=	-laesnd
LIBS	+=	-logc -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(PORTLIBS)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/bin/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
export LD	:=	$(CC)
else
export LD	:=	$(CXX)
endif

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:= -L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) install clean

#---------------------------------------------------------------------------------
# set up portlibs directory
#---------------------------------------------------------------------------------
export PORTLIBS_DIR	:=	$(DESTDIR)
ifeq ($(PLATFORM),gamecube)
ifeq ($(LIBOGC_VER),libogc)
PORTLIBS_DIR	+=	$(PORTLIBS_PATH)/gamecube
else
PORTLIBS_DIR	+=	$(DEVKITPRO)/$(LIBOGC_VER)/portlibs/gamecube
endif
else
ifeq ($(LIBOGC_VER),libogc)
PORTLIBS_DIR	+=	$(PORTLIBS_PATH)/wii
else
PORTLIBS_DIR	+=	$(DEVKITPRO)/$(LIBOGC_VER)/portlibs/wii
endif
endif

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@[ -d $(CURDIR)/bin ] || mkdir -p $(CURDIR)/bin
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
install: $(BUILD)
	@[ -d $(PORTLIBS_DIR)/include ] || mkdir -p $(PORTLIBS_DIR)/include
	@[ -d $(PORTLIBS_DIR)/lib ] || mkdir -p $(PORTLIBS_DIR)/lib

	cp -f $(CURDIR)/include/audiogc.hpp $(PORTLIBS_DIR)/include/audiogc.hpp

	cp -f $(OUTPUT).a $(PORTLIBS_DIR)/lib/$(TARGET).a

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) bin

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).a: $(OFILES)

$(OFILES_SOURCES) : $(HFILES)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

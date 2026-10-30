.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error Set DEVKITPPC in your environment)
endif
include $(DEVKITPPC)/wii_rules

.RECIPEPREFIX := >
TARGET := boot
BUILD := build
SOURCES := source
CFLAGS := -g -O2 -Wall $(MACHDEP) -I$(LIBOGC_INC)
LDFLAGS := -g $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS := -logc -lwiiuse -lbte -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(CURDIR)/$(SOURCES)
export OFILES := $(notdir $(patsubst %.c,%.o,$(wildcard $(SOURCES)/*.c)))
export LD := $(CC)
export LIBPATHS := -L$(LIBOGC_LIB)
.PHONY: all clean
all: $(BUILD)
> @$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
$(BUILD):
> @mkdir -p $@
clean:
> rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).dol $(OUTPUT).map
else
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
%.o: %.c
> $(CC) $(CFLAGS) -c $< -o $@
endif

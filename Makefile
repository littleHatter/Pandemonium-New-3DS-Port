.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set; use a devkitPro 3ds-dev shell)
endif
include $(DEVKITARM)/3ds_rules
TARGET := pandemonium
BUILD := build
APP_TITLE := Pandemonium!
APP_DESCRIPTION := Pandemonium!
APP_AUTHOR := Pandemonium compatibility project
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CPPFLAGS := -D__3DS__ -I. -I$(CTRULIB)/include -I$(DEVKITPRO)/portlibs/3ds/include
CFLAGS := -O2 -g -Wall -Wno-unused-parameter -mword-relocations -ffunction-sections $(ARCH) $(CPPFLAGS)
CXXFLAGS := $(CFLAGS) -std=gnu++17 -Wno-psabi
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map -Wl,--gc-sections
SOURCES := main_3ds runtime_s3e loader heap cpu_native guest_interp movie_player decoder
OBJECTS := $(addprefix $(BUILD)/,$(addsuffix .o,$(SOURCES))) $(BUILD)/native_arm.o $(BUILD)/LzmaDec.o
.PHONY: all clean
all: $(TARGET).3dsx
$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	3dsxtool $< $@ --smdh=$(TARGET).smdh
$(TARGET).smdh: bottom_art.h icon.png Makefile
	smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" icon.png $@
$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ -L$(CTRULIB)/lib -L$(DEVKITPRO)/portlibs/3ds/lib -lmpg123 -lz -lctru -lm -o $@
$(BUILD):
	mkdir -p $@
$(BUILD)/main_3ds.o: bottom_art.h
bottom_art.h: generate_art.py $(wildcard artwork/*.png)
	python3 generate_art.py
$(BUILD)/main_3ds.o: display_toggle_art.h
display_toggle_art.h: artwork/Switch_to_Wide.png artwork/Switch_to_Standard.png generate_display_toggle.py
	python3 generate_display_toggle.py

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) -MMD -MP $(CXXFLAGS) -c $< -o $@
$(BUILD)/native_arm.o: native_arm.S | $(BUILD)
	$(CC) -MMD -MP -x assembler-with-cpp $(ARCH) $(CPPFLAGS) -c $< -o $@
$(BUILD)/LzmaDec.o: third_party/lzma/LzmaDec.c | $(BUILD)
	$(CC) -MMD -MP $(CFLAGS) -c $< -o $@
clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh
-include $(wildcard $(BUILD)/*.d)

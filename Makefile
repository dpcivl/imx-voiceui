# Copyright 2021 NXP

CFLAGS += -DNDEBUG -DRDSP_DISABLE_FILEIO -Wno-format-security

CPLUS_FLAGS =

INSTALLDIR := ./release

BUILD_ARCH ?= CortexA53
export BUILD_ARCH

all: VOICESEEKER VOICE_UI_APP

VOICESEEKER: | $(INSTALLDIR)
	@echo "--- Build voiceseeker library ---"
	make -C ./voiceseeker/
	cp ./voiceseeker/build/$(BUILD_ARCH)/libvoiceseekerlight.so $(INSTALLDIR)/libvoiceseekerlight.so.2.0
	cp ./voiceseeker/src/Config.ini $(INSTALLDIR)/

VOICE_UI_APP: | $(INSTALLDIR)
	@echo "--- Build voice ui app ---"
	make -C ./voice_ui_app
	cp ./voice_ui_app/build/$(BUILD_ARCH)/voice_ui_app $(INSTALLDIR)/

$(INSTALLDIR) :
	mkdir $@

clean:
	rm -rf ./release
	make -C ./voiceseeker clean
	make -C ./voice_ui_app clean


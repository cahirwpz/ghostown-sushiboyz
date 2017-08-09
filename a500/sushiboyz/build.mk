TOPDIR = $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/..)

LIBS += libsys libc
CPPFLAGS += -I$(TOPDIR)/effects
LDLIBS += $(foreach lib,$(LIBS),$(TOPDIR)/base/$(lib)/$(lib).a)

include $(TOPDIR)/build.mk

%.exe: $(TOPDIR)/base/crt0.o %.o $(OBJECTS) startup.o
	@echo "[$(DIR):ld] $@"
	$(CC) $(LDFLAGS) -Wl,-Map=$@.map -o $@ $^ $(LDLIBS)
	$(CP) $@ $@.dbg
	$(STRIP) $@

%.3d: %.lwo
	@echo "[$(DIR):conv] $< -> $@"
	$(DUMPLWO) $< $@

%.ilbm: %.png
	@echo "[$(DIR):conv] $< -> $@"
	$(ILBMCONV) $< $@
	$(ILBMPACK) -f $@

%.bin: %.asm
	@echo "[$(DIR):bin] $< -> $@"
	$(AS) -Fbin -o $@ $<

%.adf: %.exe $(DATA)
	@echo "[$(DIR):adf] $^ -> $@"
	$(FSUTIL) -b $(TOPDIR)/base/bootloader.bin create $@ $^

run: all $(notdir $(PWD)).adf
	$(RUNINUAE) -e $(notdir $(PWD)).exe.dbg $(lastword $^)

clean::
	@$(RM) *.adf *.exe *.exe.{dbg,map}

.PHONY: run
.PRECIOUS: %.o


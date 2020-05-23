TOPDIR = $(realpath .)

all: build-tools build-base build-sushiboyz

include $(TOPDIR)/build.mk

clean:: clean-sushiboyz clean-base clean-tools

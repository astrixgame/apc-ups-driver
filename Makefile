topdir := .

SUBDIRS = src platforms doc
include autoconf/targets.mak

platforms_DIR: src_DIR

configure: autoconf/configure.in autoconf/aclocal.m4
	-rm -f config.cache config.log config.out config.status include/config.h
	autoconf --prepend-include=autoconf autoconf/configure.in > configure
	autoheader autoconf/configure.in
	chmod 755 configure

all: configure-usb $(SUBDIRS)

configure-usb:
	./configure --enable-usb
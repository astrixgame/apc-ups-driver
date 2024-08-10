topdir:=..

SUBDIRS = lib drivers $(USBHIDDIR) $(WIN32)
include $(topdir)/autoconf/targets.mak

# Force dirs that needs libapc.a to build after lib_DIR
# Necessary for parallel make since they link against libapc.a
win32_DIR: lib_DIR

# CGI requires win32, but only if building for win32
cgi_DIR: $(if $(WIN32),win32_DIR)

common_srcs     := options.c device.c reports.c action.c
apcupsd_srcs    := apcupsd.c apcnis.c
apcinfo_srcs  := apcinfo.c
apccmd_srcs    := apccmd.c

common_obj      := $(call SRC2OBJ,$(common_srcs))
apcupsd_obj     := $(call SRC2OBJ,$(apcupsd_srcs)) $(if $(WIN32),$(call SRC2OBJ,win32/winservice.cpp win32/winmain.cpp))
apcinfo_obj   := $(call SRC2OBJ,$(apcinfo_srcs))
apccmd_obj     := $(call SRC2OBJ,$(apccmd_srcs))

SRCS = $(common_srcs) $(apcupsd_srcs) $(apcinfo_srcs) \
       $(apccmd_srcs)

all-targets: apcupsd$(EXE) apcinfo$(EXE) apccmd$(EXE)

apcupsd$(EXE): $(common_obj) $(apcupsd_obj) $(APCDRVLIBS) $(APCLIBS)
	$(LINK) $(DRVLIBS) $(BG)

apccmd$(EXE): $(common_obj) $(apccmd_obj) $(APCDRVLIBS) $(APCLIBS)
	$(LINK) $(DRVLIBS)

apcinfo$(EXE): $(apcinfo_obj) $(APCLIBS)
	$(LINK)

all-install: install-base
all-uninstall: uninstall-base

install-base:
	$(call MKDIR,$(sbindir))
	$(call INSTPROG,700,apcupsd,$(sbindir)/apcupsd$(EXE))
	$(call INSTPROG,700,apccmd,$(sbindir)/apccmd$(EXE))
	$(call INSTPROG,755,apcinfo,$(sbindir)/apcinfo$(EXE))

uninstall-base:
	$(call UNINST,$(sbindir)/apcupsd$(EXE))
	$(call UNINST,$(sbindir)/apccmd$(EXE))
	$(call UNINST,$(sbindir)/apcinfo$(EXE))

# Include dependencies
-include $(DEPS)

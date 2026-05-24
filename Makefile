# Makefile for weather-widget — MX Linux 19 / Debian Buster i386
#
# Builds a 32-bit binary. Downloads the entire GTK stack from
# snapshot.debian.org (Buster, i386) since apt install fails due to
# MX repo package conflicts. GLib, libcurl, json-c still come from
# the system (they install cleanly on MX Linux 19).
#
# Prerequisites:
#   sudo apt install build-essential wget pkg-config gcc-multilib \
#                    libglib2.0-dev:i386 libcurl4-openssl-dev:i386 \
#                    libjson-c-dev:i386
#
# If :i386 variants of glib/curl/json-c also conflict, install the
# plain amd64 ones and the Makefile will still work — it only uses
# them for headers (which are arch-independent) and the linker will
# find the i386 .so files from deps/.
#
#   make

CC      = gcc
ARCH    = -m32
TARGET  = weather-widget
SRC     = main.c
D       = deps
LIB     = $(D)/usr/lib/i386-linux-gnu

# Use a Dec 2019 snapshot — guarantees full Buster i386 archive is present
SNAP = http://snapshot.debian.org/archive/debian/20210701T000000Z

GTK_P   = pool/main/g/gtk+3.0
PANGO_P = pool/main/p/pango1.0
ATK_P   = pool/main/a/atk1.0
CAIRO_P = pool/main/c/cairo
PIX_P   = pool/main/g/gdk-pixbuf
EPOXY_P = pool/main/libe/libepoxy
HB_P    = pool/main/h/harfbuzz
FRIB_P  = pool/main/f/fribidi
FC_P    = pool/main/f/fontconfig
FT_P    = pool/main/f/freetype
PIXMAN_P= pool/main/p/pixman
PNG_P   = pool/main/libp/libpng1.6
XEXT_P  = pool/main/libx/libxext
XFIX_P  = pool/main/libx/libxfixes
XCOM_P  = pool/main/libx/libxcomposite
XDAM_P  = pool/main/libx/libxdamage
XRAN_P  = pool/main/libx/libxrandr
XINE_P  = pool/main/libx/libxinerama
XCUR_P  = pool/main/libx/libxcursor
XREN_P  = pool/main/libx/libxrender
XI_P    = pool/main/libx/libxi
X11_P   = pool/main/libx/libx11
XCB_P   = pool/main/libx/libxcb
XAU_P   = pool/main/libx/libxau
XDMCP_P = pool/main/libx/libxdmcp
IND_P   = pool/main/libi/libindicator
APP_P   = pool/main/liba/libappindicator
DBM_P   = pool/main/libd/libdbusmenu

DEBS = \
  $(GTK_P)/libgtk-3-0_3.24.5-1_i386.deb \
  $(GTK_P)/libgtk-3-dev_3.24.5-1_i386.deb \
  $(GTK_P)/libgtk-3-common_3.24.5-1_all.deb \
  $(PANGO_P)/libpango-1.0-0_1.42.4-8~deb10u1_i386.deb \
  $(PANGO_P)/libpangocairo-1.0-0_1.42.4-8~deb10u1_i386.deb \
  $(PANGO_P)/libpangoft2-1.0-0_1.42.4-8~deb10u1_i386.deb \
  $(PANGO_P)/libpango1.0-dev_1.42.4-8~deb10u1_i386.deb \
  $(ATK_P)/libatk1.0-0_2.30.0-2_i386.deb \
  $(ATK_P)/libatk1.0-dev_2.30.0-2_i386.deb \
  $(CAIRO_P)/libcairo2_1.16.0-4+deb10u1_i386.deb \
  $(CAIRO_P)/libcairo-gobject2_1.16.0-4+deb10u1_i386.deb \
  $(CAIRO_P)/libcairo2-dev_1.16.0-4+deb10u1_i386.deb \
  $(PIX_P)/libgdk-pixbuf2.0-0_2.38.1+dfsg-1_i386.deb \
  $(PIX_P)/libgdk-pixbuf2.0-dev_2.38.1+dfsg-1_i386.deb \
  $(PIX_P)/libgdk-pixbuf2.0-common_2.38.1+dfsg-1_all.deb \
  $(EPOXY_P)/libepoxy0_1.5.3-0.1_i386.deb \
  $(EPOXY_P)/libepoxy-dev_1.5.3-0.1_i386.deb \
  $(HB_P)/libharfbuzz0b_2.3.1-1_i386.deb \
  $(HB_P)/libharfbuzz-dev_2.3.1-1_i386.deb \
  $(FRIB_P)/libfribidi0_1.0.5-3.1+deb10u1_i386.deb \
  $(FRIB_P)/libfribidi-dev_1.0.5-3.1+deb10u1_i386.deb \
  $(FC_P)/libfontconfig1_2.13.1-2_i386.deb \
  $(FC_P)/libfontconfig1-dev_2.13.1-2_i386.deb \
  $(FT_P)/libfreetype6_2.9.1-3+deb10u2_i386.deb \
  $(FT_P)/libfreetype6-dev_2.9.1-3+deb10u2_i386.deb \
  $(PIXMAN_P)/libpixman-1-0_0.36.0-1_i386.deb \
  $(PNG_P)/libpng16-16_1.6.36-6_i386.deb \
  $(XEXT_P)/libxext6_1.3.3-1+b2_i386.deb \
  $(XEXT_P)/libxext-dev_1.3.3-1+b2_i386.deb \
  $(XFIX_P)/libxfixes3_5.0.3-1_i386.deb \
  $(XFIX_P)/libxfixes-dev_5.0.3-1_i386.deb \
  $(XCOM_P)/libxcomposite1_0.4.4-2_i386.deb \
  $(XCOM_P)/libxcomposite-dev_0.4.4-2_i386.deb \
  $(XDAM_P)/libxdamage1_1.1.4-3+b3_i386.deb \
  $(XDAM_P)/libxdamage-dev_1.1.4-3+b3_i386.deb \
  $(XRAN_P)/libxrandr2_1.5.1-1_i386.deb \
  $(XRAN_P)/libxrandr-dev_1.5.1-1_i386.deb \
  $(XINE_P)/libxinerama1_1.1.4-2_i386.deb \
  $(XINE_P)/libxinerama-dev_1.1.4-2_i386.deb \
  $(XCUR_P)/libxcursor1_1.1.15-2_i386.deb \
  $(XCUR_P)/libxcursor-dev_1.1.15-2_i386.deb \
  $(XREN_P)/libxrender1_0.9.10-1_i386.deb \
  $(XREN_P)/libxrender-dev_0.9.10-1_i386.deb \
  $(XI_P)/libxi6_1.7.9-1_i386.deb \
  $(XI_P)/libxi-dev_1.7.9-1_i386.deb \
  $(X11_P)/libx11-6_1.6.7-1+deb10u2_i386.deb \
  $(X11_P)/libx11-dev_1.6.7-1+deb10u2_i386.deb \
  $(X11_P)/libx11-data_1.6.7-1+deb10u2_all.deb \
  $(XCB_P)/libxcb1_1.13.1-2_i386.deb \
  $(XAU_P)/libxau6_1.0.8-1+b2_i386.deb \
  $(XDMCP_P)/libxdmcp6_1.1.2-3_i386.deb \
  $(IND_P)/libindicator3-7_0.5.0-4_i386.deb \
  $(APP_P)/libappindicator3-1_0.4.92-7_i386.deb \
  $(APP_P)/libappindicator3-dev_0.4.92-7_i386.deb \
  $(DBM_P)/libdbusmenu-glib4_18.10.20180917~bzr490+repack1-1_i386.deb \
  $(DBM_P)/libdbusmenu-gtk3-4_18.10.20180917~bzr490+repack1-1_i386.deb \
  $(DBM_P)/libdbusmenu-glib-dev_18.10.20180917~bzr490+repack1-1_i386.deb \
  $(DBM_P)/libdbusmenu-gtk3-dev_18.10.20180917~bzr490+repack1-1_i386.deb

# ── system deps (headers are arch-independent; .so from deps/) ────────────
ifeq ($(shell pkg-config --exists glib-2.0 2>/dev/null && echo yes),yes)
    GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0 gobject-2.0)
    GLIB_LIBS   := $(shell pkg-config --libs   glib-2.0 gobject-2.0)
else
    $(error glib-2.0 not found. Run: sudo apt install libglib2.0-dev)
endif
ifeq ($(shell pkg-config --exists libcurl 2>/dev/null && echo yes),yes)
    CURL_CFLAGS := $(shell pkg-config --cflags libcurl)
    CURL_LIBS   := $(shell pkg-config --libs   libcurl)
else
    $(error libcurl not found. Run: sudo apt install libcurl4-openssl-dev)
endif
ifeq ($(shell pkg-config --exists json-c 2>/dev/null && echo yes),yes)
    JSONC_CFLAGS := $(shell pkg-config --cflags json-c)
    JSONC_LIBS   := $(shell pkg-config --libs   json-c)
else
    $(error json-c not found. Run: sudo apt install libjson-c-dev)
endif

# ── compiler / linker flags ───────────────────────────────────────────────
LOCAL_INC = \
  -I$(D)/usr/include/gtk-3.0 \
  -I$(D)/usr/include/pango-1.0 \
  -I$(D)/usr/include/atk-1.0 \
  -I$(D)/usr/include/cairo \
  -I$(D)/usr/include/gdk-pixbuf-2.0 \
  -I$(D)/usr/include/harfbuzz \
  -I$(D)/usr/include/freetype2 \
  -I$(D)/usr/include/fontconfig \
  -I$(D)/usr/include/fribidi \
  -I$(D)/usr/include/libappindicator3-0.1 \
  -I$(D)/usr/include/libdbusmenu-glib-0.4 \
  -I$(D)/usr/include/libdbusmenu-gtk3-0.4 \
  -I$(D)/usr/include \
  -I$(D)/usr/include/X11

CFLAGS  = $(ARCH) -Wall -Wextra -O2 \
          $(GLIB_CFLAGS) $(LOCAL_INC) \
          $(CURL_CFLAGS) $(JSONC_CFLAGS)

LDFLAGS = $(ARCH) \
          -L$(LIB) \
          -Wl,-rpath-link,$(LIB)

LIBS = -lgtk-3 -lgdk-3 -lcairo -lgdk_pixbuf-2.0 \
       -lappindicator3 -ldbusmenu-glib -ldbusmenu-gtk3 \
       $(GLIB_LIBS) $(CURL_LIBS) $(JSONC_LIBS)

.PHONY: all clean fetch-deps verify-urls

all: fetch-deps $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

# ── fetch-deps ────────────────────────────────────────────────────────────
fetch-deps: $(D)/.done

$(D)/.done:
	@echo ">>> Fetching Buster i386 packages from snapshot.debian.org ..."
	@echo "    (~60 packages, first run takes a few minutes)"
	@mkdir -p $(D)/debs
	@ok=1; \
	for rel in $(DEBS); do \
	    fname=$$(basename $$rel); \
	    dest=$(D)/debs/$$fname; \
	    if [ ! -f "$$dest" ]; then \
	        printf "  %-70s " "$$fname"; \
	        if wget -q --timeout=30 -O "$$dest" "$(SNAP)/$$rel"; then \
	            echo "OK"; \
	        else \
	            rm -f "$$dest"; \
	            echo "FAILED  <-- $(SNAP)/$$rel"; \
	            ok=0; \
	        fi; \
	    fi; \
	done; \
	[ "$$ok" = "1" ] || { echo ""; echo "Fix failed URLs then: make clean-deps && make"; exit 1; }; \
	echo ">>> Unpacking..."; \
	for deb in $(D)/debs/*.deb; do \
	    dpkg-deb -x "$$deb" $(D)/; \
	done; \
	echo ">>> Creating linker .so symlinks..."; \
	for v in $(LIB)/lib*.so.[0-9]*; do \
	    [ -f "$$v" ] || continue; \
	    stub=$(LIB)/$$(basename "$$v" | sed 's/\.so\.[0-9].*/\.so/'); \
	    [ -e "$$stub" ] || ln -sf "$$v" "$$stub"; \
	done; \
	touch $(D)/.done; \
	echo ">>> Ready — run: make"

verify-urls:
	@for rel in $(DEBS); do echo "$(SNAP)/$$rel"; done

clean:
	rm -f $(TARGET)

clean-deps:
	rm -rf $(D)

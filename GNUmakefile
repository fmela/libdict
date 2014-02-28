SOURCE_DIR = src
HEADER_DIR = include
OUTPUT_DIR = bin

CUNIT_PREFIX=$(HOME)/homebrew

SOURCE := $(wildcard $(SOURCE_DIR)/*.c)
HEADER := $(wildcard $(HEADER_DIR)/*.h $(SOURCE_DIR)/*.h)

STATIC_OBJ := $(SOURCE:$(SOURCE_DIR)/%.c=$(OUTPUT_DIR)/%.o)
PROFIL_OBJ := $(SOURCE:$(SOURCE_DIR)/%.c=$(OUTPUT_DIR)/%.po)
SHARED_OBJ := $(SOURCE:$(SOURCE_DIR)/%.c=$(OUTPUT_DIR)/%.So)

PROG_SRC := $(wildcard *.c)
PROGRAMS := $(PROG_SRC:%.c=$(OUTPUT_DIR)/%)
VG := $(OUTPUT_DIR)/vg

LIB := dict
STATIC_LIB_NAME := lib$(LIB).a
PROFIL_LIB_NAME := lib$(LIB)_p.a
SHARED_LIB_NAME := lib$(LIB).so
STATIC_LIB := $(OUTPUT_DIR)/$(STATIC_LIB_NAME)
PROFIL_LIB := $(OUTPUT_DIR)/$(PROFIL_LIB_NAME)
SHARED_LIB := $(OUTPUT_DIR)/$(SHARED_LIB_NAME)

# Plug in your favorite compiler here:
CC := $(shell which clang || which gcc)
INCLUDES = -I$(HEADER_DIR) -I$(SOURCE_DIR) -I$(CUNIT_PREFIX)/include
CFLAGS = -Wall -Wextra -Wshadow -W -std=c99 -O3 $(INCLUDES)
LDFLAGS =

INSTALL_PREFIX ?= /usr/local
INSTALL_BINDIR = $(INSTALL_PREFIX)/bin
INSTALL_LIBDIR = $(INSTALL_PREFIX)/lib
INSTALL_INCDIR = $(INSTALL_PREFIX)/include/dict
INSTALL_MANDIR = $(INSTALL_PREFIX)/man
INSTALL ?= install
INSTALL_LIBVER = 2
INSTALL_SHLIB = $(SHARED_LIB_NAME).$(INSTALL_LIBVER)
INSTALL_USER ?= 0
INSTALL_GROUP ?= 0

all: $(OUTPUT_DIR) $(STATIC_LIB) $(SHARED_LIB) $(PROGRAMS) $(VG)

$(OUTPUT_DIR):
	[ -d $(OUTPUT_DIR) ] || mkdir -m 755 $(OUTPUT_DIR)

$(STATIC_LIB): $(STATIC_OBJ)
	ar cru $(STATIC_LIB) $(STATIC_OBJ)

$(PROFIL_LIB): $(PROFIL_OBJ)
	ar cru $(PROFIL_LIB) $(PROFIL_OBJ)

$(SHARED_LIB): $(SHARED_OBJ)
	$(CC) -shared -o $(SHARED_LIB) $(SHARED_OBJ)

$(OUTPUT_DIR)/%.o: $(SOURCE_DIR)/%.c $(HEADER) GNUmakefile
	$(CC) $(CFLAGS) -c -o $(@) $(<)

$(OUTPUT_DIR)/%.po: $(SOURCE_DIR)/%.c $(HEADER) GNUmakefile
	$(CC) $(CFLAGS) -pg -c -o $(@) $(<)

$(OUTPUT_DIR)/%.So: $(SOURCE_DIR)/%.c $(HEADER) GNUmakefile
	$(CC) $(CFLAGS) -fPIC -DPIC -c -o $(@) $(<)

$(OUTPUT_DIR)/unit_tests: unit_tests.c $(STATIC_LIB) GNUmakefile
	$(CC) $(CFLAGS) -o $(@) $(<) $(STATIC_LIB) -L$(CUNIT_PREFIX)/lib $(LDFLAGS) -lcunit

$(OUTPUT_DIR)/%: %.c $(STATIC_LIB) GNUmakefile
	$(CC) $(CFLAGS) -o $(@) $(<) $(STATIC_LIB) $(LDFLAGS)

$(VG): vg
	cp vg $(VG)

.PHONY: clean
clean:
	if test -d $(OUTPUT_DIR); then rm -r $(OUTPUT_DIR); fi

.PHONY: analyze
analyze:
	@for x in $(SOURCE) $(PROG_SRC); \
		do echo Analyzing $$x ...; \
		clang --analyze $(INCLUDES) $$x -o /dev/null; \
	done

.PHONY: install
install: $(STATIC_LIB) $(PROFIL_LIB) $(SHARED_LIB)
	[ -d $(INSTALL_INCDIR) ] || mkdir -p -m 755 $(INSTALL_INCDIR)
	$(INSTALL) -o $(INSTALL_USER) -g $(INSTALL_GROUP) -m 644 $(HEADER) $(INSTALL_INCDIR)
	[ -d $(INSTALL_LIBDIR) ] || mkdir -p -m 755 $(INSTALL_LIBDIR)
	$(INSTALL) -s -o $(INSTALL_USER) -g $(INSTALL_GROUP) -m 644 $(STATIC_LIB) $(INSTALL_LIBDIR)/$(STATIC_LIB_NAME)
	$(INSTALL) -s -o $(INSTALL_USER) -g $(INSTALL_GROUP) -m 644 $(PROFIL_LIB) $(INSTALL_LIBDIR)/$(PROFIL_LIB_NAME)
	$(INSTALL) -s -o $(INSTALL_USER) -g $(INSTALL_GROUP) -m 755 $(SHARED_LIB) $(INSTALL_LIBDIR)/$(INSTALL_SHLIB)
	$(SHELL) -ec 'cd $(INSTALL_LIBDIR) && ln -sf $(INSTALL_SHLIB) $(SHARED_LIB_NAME)'

.PHONY: uninstall
uninstall :
	-rm -rf $(INSTALL_INCDIR)
	-rm -f $(INSTALL_LIBDIR)/$(STATIC_LIB)
	-rm -f $(INSTALL_LIBDIR)/$(PROFIL_LIB)
	-rm -f $(INSTALL_LIBDIR)/$(INSTALL_SHLIB)
	-rm -f $(INSTALL_LIBDIR)/$(SHARED_LIB)

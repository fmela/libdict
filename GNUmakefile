SOURCE_DIR = src
HEADER_DIR = include
OUTPUT_DIR = bin

SOURCE := $(wildcard $(SOURCE_DIR)/*.c)
HEADER := $(wildcard $(HEADER_DIR)/*.h $(SOURCE_DIR)/*.h)

STATIC_OBJ = $(SOURCE:$(SOURCE_DIR)/%.c=$(OUTPUT_DIR)/%.o)
PROFIL_OBJ = $(SOURCE:$(SOURCE_DIR)/%.c=$(OUTPUT_DIR)/%.po)
SHARED_OBJ = $(SOURCE:$(SOURCE_DIR)/%.c=$(OUTPUT_DIR)/%.So)

PROG_SRC = $(wildcard *.c)
PROG_OBJ = $(PROG_SRC:%.c=$(OUTPUT_DIR)/%.o)
PROG = $(PROG_SRC:%.c=$(OUTPUT_DIR)/%)

LIB = dict
STATIC_LIB = $(OUTPUT_DIR)/lib$(LIB).a
PROFIL_LIB = $(OUTPUT_DIR)/lib$(LIB)_p.a
SHARED_LIB = $(OUTPUT_DIR)/lib$(LIB).so

CFLAGS = -Wall -Wextra -ansi -pedantic -g -I$(HEADER_DIR) -I$(SOURCE_DIR)# -m32
CFLAGS:= $(CFLAGS) -O4
LDFLAGS =# -m32

OS=$(shell uname)
ifeq ($(OS),Darwin)
	CC=/Developer/usr/bin/clang
#	CFLAGS := $(CFLAGS) --analyze
else
	CC=gcc
endif

AR = ar
ARFLAGS = cru

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCDIR = $(PREFIX)/include/dict
MANDIR = $(PREFIX)/man
INSTALL ?= install
LIBVER = 2
SHLIB = $(SHARED_LIB).$(LIBVER)
USER ?= 0
GROUP ?= 0

all: $(STATIC_LIB) $(PROG) $(PROG_OBJ)

$(STATIC_LIB): $(STATIC_OBJ)
	$(AR) $(ARFLAGS) $(STATIC_LIB) $(STATIC_OBJ)

$(PROFIL_LIB): $(PROFIL_OBJ)
	$(AR) $(ARFLAGS) $(PROFIL_LIB) $(PROFIL_OBJ)

$(SHARED_LIB): $(SHARED_OBJ)
	$(CC) -shared -o $(SHARED_LIB) $(SHARED_OBJ)

$(OUTPUT_DIR)/%.o: $(SOURCE_DIR)/%.c $(HEADER)
	$(CC) $(CFLAGS) -c -o $(@) $(<)

$(OUTPUT_DIR)/%.po: $(SOURCE_DIR)/%.c $(HEADER)
	$(CC) $(CFLAGS) -pg -c -o $(@) $(<)

$(OUTPUT_DIR)/%.So: $(SOURCE_DIR)/%.c $(HEADER)
	$(CC) $(CFLAGS) -fPIC -DPIC -c -o $(@) $(<)

$(OUTPUT_DIR)/%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c -o $(@) $(<)

$(OUTPUT_DIR)/%: $(OUTPUT_DIR)/%.o $(STATIC_OBJ)
	$(CC) -o $(@) $(<) $(STATIC_OBJ) $(LDFLAGS)

.PHONY: clean
clean :
	rm -f $(STATIC_LIB) $(STATIC_OBJ)
	rm -f $(PROFIL_LIB) $(PROFIL_OBJ)
	rm -f $(SHARED_LIB) $(SHARED_OBJ)
	rm -f $(PROG) $(PROG_OBJ)
	rm -rf $(OUTPUT_DIR)/*.dSYM

install: $(STATIC_LIB) $(PROFIL_LIB) $(SHARED_LIB)
	[ -d $(INCDIR) ] || mkdir -m 755 $(INCDIR)
	$(INSTALL) -o $(USER) -g $(GROUP) -m 644 $(HEADER) $(INCDIR)
	$(INSTALL) -s -o $(USER) -g $(GROUP) -m 644 $(STATIC_LIB) $(LIBDIR)
	$(INSTALL) -s -o $(USER) -g $(GROUP) -m 644 $(PROFIL_LIB) $(LIBDIR)
	$(INSTALL) -s -o $(USER) -g $(GROUP) -m 755 $(SHARED_LIB) $(LIBDIR)/$(SHLIB)
	$(SHELL) -ec 'cd $(LIBDIR) && ln -sf $(SHLIB) $(SHARED_LIB)'

uninstall :
	-rm -rf $(INCDIR)
	-rm -f $(LIBDIR)/$(STATIC_LIB)
	-rm -f $(LIBDIR)/$(SHARED_LIB)

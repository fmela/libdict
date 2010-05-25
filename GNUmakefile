SOURCE_DIR = src
HEADER_DIR = include
OUTPUT_DIR = bin

SOURCE := $(wildcard $(SOURCE_DIR)/*.c)
HEADER := $(wildcard $(HEADER_DIR)/*.h $(SOURCE_DIR)/*.h)

STATIC_OBJ = $(SOURCE:$(SOURCE_DIR)/%.c=$(OUTPUT_DIR)/%.o)
PROFIL_OBJ = $(SOURCE:$(SOURCE_DIR)/%.c=$(OUTPUT_DIR)/%.po)
SHARED_OBJ = $(SOURCE:$(SOURCE_DIR)/%.c=$(OUTPUT_DIR)/%.So)
PROG_SRC = $(wildcard *.c)
PROG = $(PROG_SRC:%.c=$(OUTPUT_DIR)/%)

LIB = dict
STATIC_LIB = $(OUTPUT_DIR)/lib$(LIB).a
PROFIL_LIB = $(OUTPUT_DIR)/lib$(LIB)_p.a
SHARED_LIB = $(OUTPUT_DIR)/lib$(LIB).so

OS=$(shell uname)
ifeq ($(OS),Darwin)
	CC=/Developer/usr/bin/clang
else
	CC=gcc
endif
CFLAGS = -Wall -W -ansi -pedantic -g -O2 -I$(HEADER_DIR) -I$(SOURCE_DIR)
#CFLAGS = -Wall -W -ansi -pedantic -g -I$(HEADER_DIR) -I$(SOURCE_DIR)

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

all: $(STATIC_LIB) $(PROG)

$(STATIC_LIB): $(STATIC_OBJ)
	$(AR) $(ARFLAGS) $(STATIC_LIB) $(STATIC_OBJ)

$(PROFIL_LIB): $(PROFIL_OBJ)
	$(AR) $(ARFLAGS) $(PROFIL_LIB) $(PROFIL_OBJ)

$(SHARED_LIB): $(SHARED_OBJ)
	$(CC) -shared -o $(SHARED_LIB) $(SHARED_OBJ)

$(OUTPUT_DIR)/%.o: $(SOURCE_DIR)/%.c $(HEADER)
	$(CC) $(CFLAGS) -c $(<) -o $(@)

$(OUTPUT_DIR)/%.po: $(SOURCE_DIR)/%.c $(HEADER)
	$(CC) $(CFLAGS) -pg -c $(<) -o $(@)

$(OUTPUT_DIR)/%.So: $(SOURCE_DIR)/%.c $(HEADER)
	$(CC) $(CFLAGS) -fPIC -DPIC -c $(<) -o $(@)

.PHONY: clean
clean :
	rm -f $(STATIC_LIB) $(STATIC_OBJ)
	rm -f $(PROFIL_LIB) $(PROFIL_OBJ)
	rm -f $(SHARED_LIB) $(SHARED_OBJ)
	rm -f $(PROG)

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

$(OUTPUT_DIR)/%: %.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(@) $(<) $(STATIC_LIB)

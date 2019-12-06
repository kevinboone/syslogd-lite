NAME    := syslogd-lite
VERSION := 0.1
CC      :=  gcc 
LIBS    :=  -lpthread
TARGET	:= syslogd
SOURCES := $(shell find src/ -type f -name *.c)
OBJECTS := $(patsubst src/%,build/%,$(SOURCES:.c=.o))
DEPS	:= $(OBJECTS:.o=.deps)
DESTDIR := /
PREFIX  := /
SHAREDIR := $(PREFIX)/share/$(NAME)
MANDIR  := $(SHAREDIR)/man
BINDIR  := $(PREFIX)/bin
SHARE   := /$(PREFIX)/share/$(TARGET)
CFLAGS  := -fpie -fpic -Wall -DSHAREDIR=\"${SHAREDIR}\" -DNAME=\"$(NAME)\" -DVERSION=\"$(VERSION)\" -g -I include ${EXTRA_CFLAGS}
LDFLAGS := -pie  ${EXTRA_LDFLAGS}

all: $(TARGET)

$(TARGET): $(OBJECTS) 
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS) 

build/%.o: src/%.c
	@mkdir -p build/
	$(CC) $(CFLAGS) -MD -MF $(@:.o=.deps) -c -o $@ $<

clean:
	@echo "  Cleaning..."; $(RM) -r build/ $(TARGET) 

install: $(TARGET)
	mkdir -p $(DESTDIR)/$(BINDIR)
	cp -p $(TARGET) $(DESTDIR)/${BINDIR}
	#mkdir -p $(DESTDIR)/$(MANDIR)/man1
	#cp -p man1/* $(DESTDIR)/${MANDIR}/man1/
	#mkdir -p $(DESTDIR)/$(SHAREDIR)
	#cp -p share/* $(DESTDIR)/${SHAREDIR}/

-include $(DEPS)

.PHONY: clean


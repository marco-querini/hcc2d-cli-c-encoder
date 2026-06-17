CC ?= cc
CFLAGS ?= -O2
LDFLAGS ?=
LDLIBS ?= -lz
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
MAN1DIR ?= $(MANDIR)/man1
INSTALL ?= install

TARGET := hcc2d_encoder
SRC := single_file_c_hcc2d_encoder_v0.9.0.c
MANPAGE := hcc2d_encoder.1

.PHONY: all clean checksum install uninstall

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET) *.o

checksum:
	sha256sum $(SRC) LICENSE README.md CHANGELOG.md Makefile > SHA256SUMS.txt

install: $(TARGET) $(MANPAGE)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) -m 0755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"
	$(INSTALL) -d "$(DESTDIR)$(MAN1DIR)"
	$(INSTALL) -m 0644 $(MANPAGE) "$(DESTDIR)$(MAN1DIR)/$(MANPAGE)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(TARGET)"
	rm -f "$(DESTDIR)$(MAN1DIR)/$(MANPAGE)"

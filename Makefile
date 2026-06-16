CC ?= cc
CFLAGS ?= -O2
LDFLAGS ?=
LDLIBS ?= -lz

TARGET := hcc2d_encoder
SRC := single_file_c_hcc2d_encoder_v0.9.0.c

.PHONY: all clean checksum

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET) *.o

checksum:
	sha256sum $(SRC) LICENSE README.md CHANGELOG.md Makefile > SHA256SUMS.txt

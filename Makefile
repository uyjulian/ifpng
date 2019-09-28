#############################################
##                                         ##
##    Copyright (C) 2019-2019 Julian Uy    ##
##  https://sites.google.com/site/awertyb  ##
##                                         ##
##   See details of license at "LICENSE"   ##
##                                         ##
#############################################

CC = i686-w64-mingw32-gcc
CXX = i686-w64-mingw32-g++
AR = i686-w64-mingw32-ar
ASM = nasm
WINDRES = i686-w64-mingw32-windres
GIT_TAG := $(shell git describe --abbrev=0 --tags)
INCFLAGS += -I. -I.. -Iexternal/zlib -Iexternal/libpng
ALLSRCFLAGS += $(INCFLAGS) -DGIT_TAG=\"$(GIT_TAG)\"
ASMFLAGS += $(ALLSRCFLAGS) -fwin32 -DWIN32
CFLAGS += -O3 -flto
CFLAGS += $(ALLSRCFLAGS) -Wall -Wno-unused-value -Wno-format -DNDEBUG -DWIN32 -D_WIN32 -D_WINDOWS 
CFLAGS += -D_USRDLL -DUNICODE -D_UNICODE 
CXXFLAGS += $(CFLAGS) -fpermissive
WINDRESFLAGS += $(ALLSRCFLAGS) --codepage=65001
LDFLAGS += -static -static-libgcc -shared -Wl,--kill-at
LDLIBS +=

%.o: %.c
	@printf '\t%s %s\n' CC $<
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cpp
	@printf '\t%s %s\n' CXX $<
	$(CXX) -c $(CXXFLAGS) -o $@ $<

%.o: %.asm
	@printf '\t%s %s\n' ASM $<
	$(ASM) $(ASMFLAGS) $< -o$@ 

%.o: %.rc
	@printf '\t%s %s\n' WINDRES $<
	$(WINDRES) $(WINDRESFLAGS) $< $@

LIBZ_SOURCES += external/zlib/adler32.c external/zlib/compress.c external/zlib/crc32.c external/zlib/deflate.c external/zlib/gzclose.c external/zlib/gzlib.c external/zlib/gzread.c external/zlib/gzwrite.c external/zlib/infback.c external/zlib/inffast.c external/zlib/inflate.c external/zlib/inftrees.c external/zlib/trees.c external/zlib/uncompr.c external/zlib/zutil.c
LIBPNG_SOURCES += external/libpng/png.c external/libpng/pngerror.c external/libpng/pngget.c external/libpng/pngmem.c external/libpng/pngpread.c external/libpng/pngread.c external/libpng/pngrio.c external/libpng/pngrtran.c external/libpng/pngrutil.c external/libpng/pngset.c external/libpng/pngtrans.c external/libpng/pngwio.c external/libpng/pngwrite.c external/libpng/pngwtran.c external/libpng/pngwutil.c
SOURCES := extractor.c spi00in.c ifpng.rc $(LIBZ_SOURCES) $(LIBPNG_SOURCES)
OBJECTS := $(SOURCES:.c=.o)
OBJECTS := $(OBJECTS:.cpp=.o)
OBJECTS := $(OBJECTS:.asm=.o)
OBJECTS := $(OBJECTS:.rc=.o)

BINARY ?= ifpng.spi
ARCHIVE ?= ifpng.$(GIT_TAG).7z

all: $(BINARY)

archive: $(ARCHIVE)

clean:
	rm -f $(OBJECTS) $(BINARY) $(ARCHIVE)

$(ARCHIVE): $(BINARY) 
	rm -f $(ARCHIVE)
	7z a $@ $^

$(BINARY): $(OBJECTS) 
	@printf '\t%s %s\n' LNK $@
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

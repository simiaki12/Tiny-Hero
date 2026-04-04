CC       = x86_64-w64-mingw32-gcc
CXX_HOST = g++
CFLAGS   = -Wall -Wextra -std=c11
DEBUGFLAGS   = -g
RELEASEFLAGS = -Os -flto
LDFLAGS        = -lgdi32 -mwindows
LDFLAGS_STATIC = $(LDFLAGS) -static-libgcc

SRC    = $(wildcard src/*.c)
OUT    = build/game.exe
PACKER = build/packer

MINGW_DLL_DIR = /usr/lib/gcc/x86_64-w64-mingw32/10-win32

debug: pack
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)
	cp $(MINGW_DLL_DIR)/libgcc_s_seh-1.dll build/

release: pack
	$(CC) $(CFLAGS) $(RELEASEFLAGS) $(SRC) -o $(OUT) $(LDFLAGS_STATIC) -s
	ls -lh $(OUT)

packer:
	mkdir -p build
	$(CXX_HOST) -std=c++17 -Os tools/packer.cpp -o $(PACKER)

pack: packer
	$(PACKER)

clean:
	rm -rf build data.pak

CC       = x86_64-w64-mingw32-gcc
CXX_HOST = g++
CC_HOST  = gcc
CFLAGS   = -Wall -Wextra -std=c11
DEBUGFLAGS   = -g
RELEASEFLAGS = -Os -flto
LDFLAGS        = -lgdi32 -mwindows
LDFLAGS_STATIC = $(LDFLAGS) -static-libgcc

SRC    = $(wildcard src/*.c)
OUT    = build/game.exe
PACKER     = build/packer
MAP_EDITOR = build/map_editor
PLR_EDITOR = build/player_editor

debug: pack
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

release: pack
	$(CC) $(CFLAGS) $(RELEASEFLAGS) $(SRC) -o $(OUT) $(LDFLAGS_STATIC) -s
	ls -lh $(OUT)

packer:
	mkdir -p build
	$(CXX_HOST) -std=c++17 -Os tools/packer.cpp -o $(PACKER)

pack: packer
	$(PACKER)

map_editor:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os -Wno-unused-result tools/map_editor.c -o $(MAP_EDITOR) -lncurses

player_editor:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os -Wno-unused-result tools/player_editor.c -o $(PLR_EDITOR) -lncurses

tools: map_editor player_editor

clean:
	rm -rf build data.pak

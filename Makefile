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
DLG_EDITOR = build/dialog_editor

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

dialog_editor:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os -Wno-unused-result tools/dialog_editor.c -o $(DLG_EDITOR) -lncurses

seed_dialogs:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/seed_dialogs.c -o build/seed_dialogs
	./build/seed_dialogs

seed_enemies:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/seed_enemies.c -o build/seed_enemies
	./build/seed_enemies

migrate_maps:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/migrate_map.c -o build/migrate_map
	./build/migrate_map assets/map1.bin

tools: map_editor player_editor dialog_editor

clean:
	rm -rf build data.pak

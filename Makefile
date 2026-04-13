CC       = x86_64-w64-mingw32-gcc
CXX_HOST = g++
CC_HOST  = gcc
CFLAGS   = -Wall -Wextra -std=c11
DEBUGFLAGS   = -g
RELEASEFLAGS = -Os -flto
LDFLAGS        = -lgdi32 -lwinmm -mwindows
LDFLAGS_STATIC = $(LDFLAGS) -static-libgcc

SRC    = $(wildcard src/*.c)
OUT    = build/game.exe
PACKER      = build/packer
MAP_EDITOR  = build/map_editor
PLR_EDITOR  = build/player_editor
DLG_EDITOR  = build/dialog_editor
QST_EDITOR  = build/quest_editor
IMG_CONV    = build/img_conv
BW_CONV     = build/bw_conv
RLE         = build/rle

debug: pack
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

release: pack
	$(CC) $(CFLAGS) $(RELEASEFLAGS) $(SRC) -o $(OUT) $(LDFLAGS_STATIC) -s
	ls -lh $(OUT)

gen_world_music:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/gen_world_music.c -o build/gen_world_music
	./build/gen_world_music

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

quest_editor:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os -Wno-unused-result tools/quest_editor.c -o $(QST_EDITOR) -lncurses

seed_quests:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/seed_quests.c -o build/seed_quests
	./build/seed_quests

seed_dialogs:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/seed_dialogs.c -o build/seed_dialogs
	./build/seed_dialogs

seed_player:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/seed_player.c -o build/seed_player
	./build/seed_player

seed_items:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/seed_items.c -o build/seed_items
	./build/seed_items

seed_loottables:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/seed_loottables.c -o build/seed_loottables
	./build/seed_loottables

seed_enemies:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/seed_enemies.c -o build/seed_enemies
	./build/seed_enemies

migrate_maps:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/migrate_map.c -o build/migrate_map
	./build/migrate_map assets/map1.bin

img_conv:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/img_conv.c -o $(IMG_CONV) -lm

bw_conv:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/bw_conv.c -o $(BW_CONV) -lm

rle:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os tools/rle.c -o $(RLE)

item_editor:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os -Wno-unused-result tools/item_editor.c -o build/item_editor -lncurses

loottable_editor:
	mkdir -p build
	$(CC_HOST) -std=c11 -Os -Wno-unused-result tools/loottable_editor.c -o build/loottable_editor -lncurses

tools: map_editor player_editor dialog_editor quest_editor item_editor loottable_editor img_conv bw_conv rle

clean:
	rm -rf build data.pak

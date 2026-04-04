CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
DEBUGFLAGS = -g
RELEASEFLAGS = -Os -fno-exceptions -fno-rtti
LDFLAGS = -s
SDLFLAGS = $(shell sdl2-config --cflags --libs)

SRC = $(wildcard src/*.cpp src/*/*.cpp)
OUT = build/game
PACKER = build/packer

debug: pack
	$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) $(SRC) -o $(OUT) $(SDLFLAGS)

release: pack
	$(CXX) $(CXXFLAGS) $(RELEASEFLAGS) $(SRC) -o $(OUT) $(SDLFLAGS) $(LDFLAGS)
	strip $(OUT)
	ls -lh $(OUT)

packer:
	mkdir -p build
	$(CXX) -std=c++17 -Os tools/packer.cpp -o $(PACKER)

pack: packer
	$(PACKER)

clean:
	rm -rf build data.pak
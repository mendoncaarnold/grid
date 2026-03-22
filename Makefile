CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
SDL_CF   := $(shell sdl2-config --cflags)
SDL_LF   := $(shell sdl2-config --libs) -lSDL2_ttf

SRC := GridLayout.cpp main.cpp
OUT := GridLayoutDemo

.PHONY: all clean
all:
	$(CXX) $(CXXFLAGS) $(SDL_CF) $(SRC) -o $(OUT) $(SDL_LF)
	@echo "Build OK  ->  ./$(OUT) [grid.mock.json]"
clean:
	rm -f $(OUT)
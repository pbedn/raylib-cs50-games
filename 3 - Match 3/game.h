#pragma once
#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

// --- Virtual resolution (mirrors CS50) ---
enum { VIRTUAL_W = 512, VIRTUAL_H = 288 };

// --- Board configuration ---
enum {
    BOARD_COLS = 8,
    BOARD_ROWS = 8,
    TILE_SIZE  = 32,
    TILE_COLORS = 18,       // CS50: 18 colors
    TILE_VARIETIES = 6      // CS50: 6 varieties per color
};

#define BOARD_OFFSET_X 128  // pixels from left edge
#define BOARD_OFFSET_Y 32   // optional top margin

// --- Forward declarations of opaque types used across states ---
typedef struct Tile Tile;
typedef struct Board Board;

// --- State interface (minimal, Lua-like) ---
typedef struct State State;
struct State {
    void *data;
    void (*enter)(State *s, void *param);
    void (*exit)(State *s);
    void (*update)(State *s, float dt);
    void (*render)(State *s);
};

typedef struct StateMachine {
    State *current;
} StateMachine;

// --- Global states (declared in game.c) ---
extern State gStart, gBegin, gPlay, gOver;
extern StateMachine gSM;

// --- Game lifecycle (declared in game.c) ---
bool Game_Init(void);
void Game_Shutdown(void);
void Game_Update(float dt);
void Game_Render(void);

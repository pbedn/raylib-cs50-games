// game.c
// CS50 Match-3 — swap1 (“The Static Swap”) with real graphics
// Mirrors main.lua behavior: 8x8 board, arrow-key selection, Enter to highlight & swap.
// Graphics: loads match3.png, generates 32x32 quads, draws sprites with highlight/selection.
//
// Controls:
//   Arrows  - move selection
//   Enter   - first press: highlight; second press: swap highlighted with current
//   Esc     - cancel highlight
//   Esc (window) - close window
//
// Build (Windows/MinGW example):
//   gcc game.c -o match3.exe -std=c99 -Wall -Wextra -I. -lraylib -lopengl32 -lgdi32 -lwinmm -lm
//
// Place `match3.png` next to the executable (same dir).
// Virtual resolution matches Lua: 512x288; board drawn at offset (128,16).

#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

// ----------------- Virtual resolution & window -----------------
static const int VIRTUAL_W = 512;
static const int VIRTUAL_H = 288;

static const int WINDOW_W  = 1280;
static const int WINDOW_H  = 720;

static RenderTexture2D target;

// ----------------- Board configuration -----------------
#define BOARD_ROWS 8
#define BOARD_COLS 8
#define TILE_SIZE  32

// Drawing offset (like Lua drawBoard(128, 16))
static const int BOARD_OFFSET_X = 128;
static const int BOARD_OFFSET_Y = 16;

// ----------------- Tileset / quads -----------------
typedef struct {
    Rectangle src; // source rect inside the tileset texture
} Quad;

typedef struct {
    Texture2D tex;
    Quad* quads;         // dynamic array of quads
    int quadCount;       // total number of quads
    int tileW, tileH;    // 32x32
    int sheetCols;
    int sheetRows;
} TileSet;

static TileSet gTileset = {0};

static bool LoadTileSet(const char* path, int tileW, int tileH) {
    gTileset.tex = LoadTexture(path);
    if (gTileset.tex.id == 0) return false;

    gTileset.tileW = tileW;
    gTileset.tileH = tileH;

    gTileset.sheetCols = gTileset.tex.width  / tileW;
    gTileset.sheetRows = gTileset.tex.height / tileH;
    gTileset.quadCount = gTileset.sheetCols * gTileset.sheetRows;

    gTileset.quads = (Quad*)MemAlloc(sizeof(Quad) * gTileset.quadCount);
    if (!gTileset.quads) return false;

    int idx = 0;
    for (int y = 0; y < gTileset.sheetRows; ++y) {
        for (int x = 0; x < gTileset.sheetCols; ++x) {
            gTileset.quads[idx++].src = (Rectangle){
                (float)(x * tileW),
                (float)(y * tileH),
                (float)tileW,
                (float)tileH
            };
        }
    }

    // Pixelated scaling like love.setDefaultFilter('nearest', 'nearest')
    SetTextureFilter(gTileset.tex, TEXTURE_FILTER_POINT);
    return true;
}

static void UnloadTileSet(void) {
    if (gTileset.quads) { MemFree(gTileset.quads); gTileset.quads = NULL; }
    if (gTileset.tex.id) UnloadTexture(gTileset.tex);
    gTileset = (TileSet){0};
}

// ----------------- Board model -----------------
typedef struct {
    // Grid coords are 1..8 like in Lua
    int gridX;
    int gridY;
    // Pixel coords are (x,y) = ( (gridX-1)*32, (gridY-1)*32 )
    float x;
    float y;
    // Quad index into tileset.quads (1..quadCount in Lua; here we store 0..quadCount-1)
    int quadIndex;
} Tile;

typedef struct {
    Tile tiles[BOARD_ROWS][BOARD_COLS]; // [row][col] = [y-1][x-1]
} Board;

static Board board;

// Selected (cursor) and highlighted (first chosen tile)
static int selGX = 1, selGY = 1;   // selected tile grid coords
static bool hasHighlight = false;
static int hiGX = 1, hiGY = 1;

// ----------------- Utility -----------------
static inline Tile* TileAt(int gx, int gy) {
    if (gx < 1 || gx > BOARD_COLS || gy < 1 || gy > BOARD_ROWS) return NULL;
    return &board.tiles[gy - 1][gx - 1];
}

static inline void UpdateTileXY(Tile *t) {
    t->x = (float)((t->gridX - 1) * TILE_SIZE);
    t->y = (float)((t->gridY - 1) * TILE_SIZE);
}

// ----------------- Board init & swap -----------------
static void BoardInit(void) {
    srand((unsigned)time(NULL));

    for (int gy = 1; gy <= BOARD_ROWS; ++gy) {
        for (int gx = 1; gx <= BOARD_COLS; ++gx) {
            Tile *t = TileAt(gx, gy);
            t->gridX = gx;
            t->gridY = gy;
            t->quadIndex = GetRandomValue(0, gTileset.quadCount - 1);
            UpdateTileXY(t);
        }
    }

    selGX = 1; selGY = 1;
    hasHighlight = false;
    hiGX = 1; hiGY = 1;
}

static void SwapTilesAt(int ax, int ay, int bx, int by) {
    if (ax == bx && ay == by) return;

    int iax = ax - 1, iay = ay - 1;
    int ibx = bx - 1, iby = by - 1;

    Tile tmp = board.tiles[iay][iax];
    board.tiles[iay][iax] = board.tiles[iby][ibx];
    board.tiles[iby][ibx] = tmp;

    // Fix logical grid coords and pixel positions
    Tile *tA = &board.tiles[iay][iax];
    Tile *tB = &board.tiles[iby][ibx];

    tA->gridX = ax; tA->gridY = ay; UpdateTileXY(tA);
    tB->gridX = bx; tB->gridY = by; UpdateTileXY(tB);
}

// ----------------- Input (swap1 logic) -----------------
static void HandleMovement(void) {
    if (IsKeyPressed(KEY_UP)    && selGY > 1)          selGY--;
    if (IsKeyPressed(KEY_DOWN)  && selGY < BOARD_ROWS) selGY++;
    if (IsKeyPressed(KEY_LEFT)  && selGX > 1)          selGX--;
    if (IsKeyPressed(KEY_RIGHT) && selGX < BOARD_COLS) selGX++;
}

static void HandleSelectionAndSwap(void) {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (!hasHighlight) {
            hasHighlight = true;
            hiGX = selGX; hiGY = selGY;
        } else {
            int aGX = selGX, aGY = selGY; // current cursor tile
            int bGX = hiGX, bGY = hiGY;   // highlighted tile
            SwapTilesAt(aGX, aGY, bGX, bGY);

            // After swap, mirror Lua: selectedTile = tile2 (now moved to aGX,aGY)
            hasHighlight = false;
            selGX = aGX; selGY = aGY;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        hasHighlight = false;
    }
}

// ----------------- Drawing -----------------
static void DrawBoard(int offsetX, int offsetY) {
    // Tiles
    for (int gy = 1; gy <= BOARD_ROWS; ++gy) {
        for (int gx = 1; gx <= BOARD_COLS; ++gx) {
            Tile *t = TileAt(gx, gy);
            // Source rect (quad)
            Rectangle src = gTileset.quads[t->quadIndex].src;
            // Destination position with offset
            Vector2 pos = { t->x + (float)offsetX, t->y + (float)offsetY };
            DrawTextureRec(gTileset.tex, src, pos, WHITE);

            // Highlighted tile fill (translucent white with rounded corners)
            if (hasHighlight && t->gridX == hiGX && t->gridY == hiGY) {
                // half opacity
                Color c = (Color){255, 255, 255, 128};
                Rectangle r = { pos.x, pos.y, TILE_SIZE, TILE_SIZE };
                DrawRectangleRounded(r, 0.20f, 4, c);
            }
        }
    }

    // Selected tile outline (thicker red rounded rectangle)
    Tile *sel = TileAt(selGX, selGY);
    if (sel) {
        Vector2 pos = { sel->x + (float)offsetX, sel->y + (float)offsetY };
        Rectangle r = { pos.x, pos.y, TILE_SIZE, TILE_SIZE };
        // almost opaque red
        Color rc = (Color){255, 0, 0, 234};
        // rounded, 1px
        DrawRectangleRoundedLines(r, 0.20f, 4, rc);

    }
}

static void DrawBackdrop(void) {
    // Simple panel to make tiles pop; sized to board area
    const int w = BOARD_COLS * TILE_SIZE;
    const int h = BOARD_ROWS * TILE_SIZE;

    Rectangle outer = { (float)BOARD_OFFSET_X - 6, (float)BOARD_OFFSET_Y - 6, (float)w + 12, (float)h + 12 };
    DrawRectangleRec(outer, (Color){25, 28, 35, 255});

    Rectangle inner = { (float)BOARD_OFFSET_X, (float)BOARD_OFFSET_Y, (float)w, (float)h };
    DrawRectangleRec(inner, (Color){15, 16, 22, 255});
}

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_W, WINDOW_H, "Match-3 — swap1 (sprites)");
    SetTargetFPS(60);

    target = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    if (!LoadTileSet("res/graphics/match3.png", 32, 32)) {
        TraceLog(LOG_ERROR, "Failed to load tileset 'match3.png'");
        CloseWindow();
        return 1;
    }

    BoardInit();

    while (!WindowShouldClose()) {
        HandleMovement();
        HandleSelectionAndSwap();

        // --- Draw to virtual backbuffer ---
        BeginTextureMode(target);
            ClearBackground((Color){18, 20, 24, 255});

            DrawText("swap1 — Static Swap (Arrows: move, Enter: select/swap, Esc: cancel)",
                     8, 4, 10, (Color){220,235,255,255});

            DrawBackdrop();
            DrawBoard(BOARD_OFFSET_X, BOARD_OFFSET_Y);
        EndTextureMode();

        // --- Present with scaling ---
        BeginDrawing();
            ClearBackground(BLACK);
            float scale = fminf(
                (float)GetScreenWidth()  / (float)VIRTUAL_W,
                (float)GetScreenHeight() / (float)VIRTUAL_H
            );
            Rectangle src = { 0, 0, (float)target.texture.width, -(float)target.texture.height };
            Rectangle dst = {
                (GetScreenWidth()  - (float)VIRTUAL_W * scale) * 0.5f,
                (GetScreenHeight() - (float)VIRTUAL_H * scale) * 0.5f,
                (float)VIRTUAL_W * scale,
                (float)VIRTUAL_H * scale
            };
            DrawTexturePro(target.texture, src, dst, (Vector2){0,0}, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadTileSet();
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}

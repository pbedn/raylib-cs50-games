// game.c
// CS50 Match-3 — swap2: "The Tween Swap"
// - Same behavior as swap1 (arrows move selection; Enter to select & swap; Esc cancels)
// - Visual difference: the two tiles tween to each other's positions before finalizing the swap
//
// Controls:
//   Arrows  - move selection
//   Enter   - first press: highlight; second press: tweened swap with highlighted tile
//   Esc     - cancel highlight
//
// Build (Windows/MinGW example):
//   gcc game.c -o match3.exe -std=c99 -Wall -Wextra -I. -DTWEEN_IMPL -lraylib -lopengl32 -lgdi32 -lwinmm -lm
//
// Assets:
//   res/graphics/match3.png   (32x32 tiles, standard CS50 match-3 sheet)

#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

#define TWEEN_IMPL
#include "tween.h"   // our header-only tween/after/chain system

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
    Rectangle src;
} Quad;

typedef struct {
    Texture2D tex;
    Quad* quads;
    int quadCount;
    int tileW, tileH;
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
    int  gridX;     // 1..BOARD_COLS
    int  gridY;     // 1..BOARD_ROWS
    float x;        // pixel (local board space)
    float y;
    int  quadIndex; // tileset quad index (0..quadCount-1)
} Tile;

typedef struct {
    Tile tiles[BOARD_ROWS][BOARD_COLS]; // [row][col] = [y-1][x-1]
} Board;

static Board board;

// Cursor & highlight
static int selGX = 1, selGY = 1;     // current selection (grid coords)
static bool hasHighlight = false;
static int hiGX = 1, hiGY = 1;       // highlighted tile (grid coords)

// Swap tween state (to block input while animating)
static bool gSwapActive = false;
typedef struct {
    int aGX, aGY;
    int bGX, bGY;
    int remaining;  // countdown; when both tweens finish -> 0
} SwapAnimCtx;
static SwapAnimCtx gSwapCtx = {0};

// ----------------- Utility -----------------
static inline Tile* TileAt(int gx, int gy) {
    if (gx < 1 || gx > BOARD_COLS || gy < 1 || gy > BOARD_ROWS) return NULL;
    return &board.tiles[gy - 1][gx - 1];
}

static inline void SetTileXYFromGrid(Tile *t) {
    t->x = (float)((t->gridX - 1) * TILE_SIZE);
    t->y = (float)((t->gridY - 1) * TILE_SIZE);
}

// ----------------- Board init & logical swap -----------------
static void BoardInit(void) {
    srand((unsigned)time(NULL));

    for (int gy = 1; gy <= BOARD_ROWS; ++gy) {
        for (int gx = 1; gx <= BOARD_COLS; ++gx) {
            Tile *t = TileAt(gx, gy);
            t->gridX = gx;
            t->gridY = gy;
            t->quadIndex = GetRandomValue(0, gTileset.quadCount - 1);
            SetTileXYFromGrid(t);
        }
    }

    selGX = 1; selGY = 1;
    hasHighlight = false;
    hiGX = 1; hiGY = 1;
    gSwapActive = false;
}

static void SwapTilesInArrayAndFix(int ax, int ay, int bx, int by) {
    if (ax == bx && ay == by) return;

    int iax = ax - 1, iay = ay - 1;
    int ibx = bx - 1, iby = by - 1;

    Tile tmp = board.tiles[iay][iax];
    board.tiles[iay][iax] = board.tiles[iby][ibx];
    board.tiles[iby][ibx] = tmp;

    Tile *tA = &board.tiles[iay][iax];
    Tile *tB = &board.tiles[iby][ibx];

    tA->gridX = ax; tA->gridY = ay; SetTileXYFromGrid(tA);
    tB->gridX = bx; tB->gridY = by; SetTileXYFromGrid(tB);
}

// ----------------- Tweened swap -----------------
static void OnSwapTweenFinished(void *ud) {
    // Decrement remaining; when both complete, finalize logical swap.
    SwapAnimCtx *ctx = (SwapAnimCtx*)ud;
    if (ctx->remaining > 0) ctx->remaining--;

    if (ctx->remaining == 0) {
        // Perform the actual logical swap in the board array and fix grid coords.
        SwapTilesInArrayAndFix(ctx->aGX, ctx->aGY, ctx->bGX, ctx->bGY);

        // After swap, selection should follow tile2 (highlighted), which moved to (aGX,aGY)
        selGX = ctx->aGX;
        selGY = ctx->aGY;

        // Clear highlight and unlock input
        hasHighlight = false;
        gSwapActive = false;
    }
}

static void StartSwapTween(int aGX, int aGY, int bGX, int bGY) {
    // Prepare animated movement of the two tiles to each other's positions.
    Tile *a = TileAt(aGX, aGY);
    Tile *b = TileAt(bGX, bGY);
    if (!a || !b) return;

    // Targets are the *other* tile's grid-based positions (local board space)
    float aTargetX = (float)((bGX - 1) * TILE_SIZE);
    float aTargetY = (float)((bGY - 1) * TILE_SIZE);
    float bTargetX = (float)((aGX - 1) * TILE_SIZE);
    float bTargetY = (float)((aGY - 1) * TILE_SIZE);

    // Mark that a swap is active
    gSwapActive = true;
    gSwapCtx = (SwapAnimCtx){ .aGX = aGX, .aGY = aGY, .bGX = bGX, .bGY = bGY, .remaining = 2 };

    // Duration & easing (feel free to tweak)
    const float dur = 0.15f;

    // Tween A
    Tween *twA = Tween_Create(dur);
    Tween_Add(twA, &a->x, aTargetX);
    Tween_Add(twA, &a->y, aTargetY);
    Tween_SetEase(twA, Tween_EaseOutQuad);
    Tween_OnFinish(twA, OnSwapTweenFinished, &gSwapCtx);
    Tween_Start(twA);

    // Tween B
    Tween *twB = Tween_Create(dur);
    Tween_Add(twB, &b->x, bTargetX);
    Tween_Add(twB, &b->y, bTargetY);
    Tween_SetEase(twB, Tween_EaseOutQuad);
    Tween_OnFinish(twB, OnSwapTweenFinished, &gSwapCtx);
    Tween_Start(twB);
}

// ----------------- Input (swap2 logic) -----------------
static void HandleMovement(void) {
    if (gSwapActive) return; // lock movement during tween

    if (IsKeyPressed(KEY_UP)    && selGY > 1)          selGY--;
    if (IsKeyPressed(KEY_DOWN)  && selGY < BOARD_ROWS) selGY++;
    if (IsKeyPressed(KEY_LEFT)  && selGX > 1)          selGX--;
    if (IsKeyPressed(KEY_RIGHT) && selGX < BOARD_COLS) selGX++;
}

static void HandleSelectionAndSwap(void) {
    if (gSwapActive) return; // lock selection during tween

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (!hasHighlight) {
            hasHighlight = true;
            hiGX = selGX; hiGY = selGY;
        } else {
            int aGX = selGX, aGY = selGY; // current cursor tile
            int bGX = hiGX, bGY = hiGY;   // highlighted tile
            StartSwapTween(aGX, aGY, bGX, bGY);
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        hasHighlight = false;
    }
}

// ----------------- Drawing -----------------
static void DrawBackdrop(void) {
    const int w = BOARD_COLS * TILE_SIZE;
    const int h = BOARD_ROWS * TILE_SIZE;

    Rectangle outer = { (float)BOARD_OFFSET_X - 6, (float)BOARD_OFFSET_Y - 6, (float)w + 12, (float)h + 12 };
    DrawRectangleRec(outer, (Color){25, 28, 35, 255});

    Rectangle inner = { (float)BOARD_OFFSET_X, (float)BOARD_OFFSET_Y, (float)w, (float)h };
    DrawRectangleRec(inner, (Color){15, 16, 22, 255});
}

static void DrawBoard(int offsetX, int offsetY) {
    // Draw tiles
    for (int gy = 1; gy <= BOARD_ROWS; ++gy) {
        for (int gx = 1; gx <= BOARD_COLS; ++gx) {
            Tile *t = TileAt(gx, gy);
            Rectangle src = gTileset.quads[t->quadIndex].src;
            Vector2 pos = { t->x + (float)offsetX, t->y + (float)offsetY };
            DrawTextureRec(gTileset.tex, src, pos, WHITE);

            // Highlighted tile translucent overlay
            if (hasHighlight && t->gridX == hiGX && t->gridY == hiGY) {
                Rectangle r = { pos.x, pos.y, TILE_SIZE, TILE_SIZE };
                DrawRectangleRounded(r, 0.20f, 4, (Color){255,255,255,100});
            }
        }
    }

    // Selected tile outline (red)
    Tile *sel = TileAt(selGX, selGY);
    if (sel) {
        Vector2 pos = { sel->x + (float)offsetX, sel->y + (float)offsetY };
        Rectangle r = { pos.x, pos.y, TILE_SIZE, TILE_SIZE };
        DrawRectangleLinesEx(r, 3.0f, (Color){255, 64, 64, 230}); // thick, non-rounded (portable)
    }
}

// ----------------- App lifecycle -----------------
int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_W, WINDOW_H, "Match-3 — swap2 (tween swap)");
    SetTargetFPS(60);

    target = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    if (!LoadTileSet("res/graphics/match3.png", 32, 32)) {
        TraceLog(LOG_ERROR, "Failed to load tileset 'res/graphics/match3.png'");
        CloseWindow();
        return 1;
    }

    Tween_InitSystem();
    BoardInit();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Tween_UpdateAll(dt);

        // Input
        HandleMovement();
        HandleSelectionAndSwap();

        // --- Draw to virtual backbuffer ---
        BeginTextureMode(target);
            ClearBackground((Color){18, 20, 24, 255});

            DrawText("swap2 — Tween Swap (Arrows: move, Enter: select/swap, Esc: cancel)",
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

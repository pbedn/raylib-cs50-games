// game.c
// CS50 Match-3 — Stage swap1: "The Static Swap"
// - Arrow keys move the selection cursor
// - Enter selects a tile (highlight). Enter on a second tile swaps the two
// - No validity rules yet (any swap is allowed)
// - Rendering uses simple colored rectangles for tiles
//
// Build (example):
//   gcc game.c -o match3 -std=c99 -Wall -Wextra -I. -DTWEEN_IMPL -lraylib -lopengl32 -lgdi32 -lwinmm -lm

#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

#define TWEEN_IMPL
#include "tween.h"   // not used yet here, included for continuity across stages

// ----------------- Virtual resolution & window -----------------
static const int VIRTUAL_W = 384;
static const int VIRTUAL_H = 216;

static const int WINDOW_W  = 1280;
static const int WINDOW_H  = 720;

static RenderTexture2D target;

// ----------------- Board configuration -----------------
#define BOARD_ROWS 8
#define BOARD_COLS 8
#define TILE_SIZE  20

typedef struct {
    int x;
    int y;
} Point;

static Point boardOrigin; // top-left pixel where board is drawn

#define COLOR_COUNT   6
#define VARIETY_COUNT 3

typedef struct {
    int gridX;   // 1..BOARD_COLS
    int gridY;   // 1..BOARD_ROWS
    int color;   // 1..COLOR_COUNT
    int variety; // 1..VARIETY_COUNT
    float x;     // pixel position (derived from origin + grid*size)
    float y;
} Tile;

typedef struct {
    Tile tiles[BOARD_ROWS][BOARD_COLS]; // [row][col] = [y-1][x-1]
} Board;

static Board board;

// Selection state (cursor) and highlight (first selected tile)
static int selGX = 1, selGY = 1;     // current cursor position (grid coords, 1-based)
static bool hasHighlight = false;    // whether a tile is currently highlighted/selected
static int hiGX = 1, hiGY = 1;       // highlighted tile grid coords (1-based)

// ----------------- Utility -----------------
static inline Color ColorFromIndex(int idx) {
    switch (idx) {
        case 1: return (Color){231,  76,  60, 255}; // red
        case 2: return (Color){ 46, 204, 113, 255}; // green
        case 3: return (Color){ 52, 152, 219, 255}; // blue
        case 4: return (Color){241, 196,  15, 255}; // yellow
        case 5: return (Color){155,  89, 182, 255}; // purple
        case 6: return (Color){230, 126,  34, 255}; // orange
        default: return (Color){200, 200, 200, 255};
    }
}

static inline Rectangle TileRectPx(int gx, int gy) {
    float x = (float)boardOrigin.x + (gx - 1) * TILE_SIZE;
    float y = (float)boardOrigin.y + (gy - 1) * TILE_SIZE;
    return (Rectangle){ x, y, (float)TILE_SIZE, (float)TILE_SIZE };
}

static inline Tile* TileAt(int gx, int gy) {
    if (gx < 1 || gx > BOARD_COLS || gy < 1 || gy > BOARD_ROWS) return NULL;
    return &board.tiles[gy - 1][gx - 1];
}

static inline void UpdateTileXY(Tile *t) {
    Rectangle r = TileRectPx(t->gridX, t->gridY);
    t->x = r.x;
    t->y = r.y;
}

// ----------------- Board init & draw -----------------
static void BoardInit(void) {
    const int w = BOARD_COLS * TILE_SIZE;
    const int h = BOARD_ROWS * TILE_SIZE;
    boardOrigin.x = (VIRTUAL_W - w) / 2;
    boardOrigin.y = (VIRTUAL_H - h) / 2;

    srand((unsigned)time(NULL));

    for (int gy = 1; gy <= BOARD_ROWS; ++gy) {
        for (int gx = 1; gx <= BOARD_COLS; ++gx) {
            Tile *t = TileAt(gx, gy);
            t->gridX = gx;
            t->gridY = gy;
            t->color = 1 + (rand() % COLOR_COUNT);
            t->variety = 1 + (rand() % VARIETY_COUNT);
            UpdateTileXY(t);
        }
    }

    // Initial selection matches CS50 idea: selectedTile = board[1][1]
    selGX = 1; selGY = 1;
    hasHighlight = false;
    hiGX = 1; hiGY = 1;
}

static void DrawBoardBackdrop(void) {
    const int w = BOARD_COLS * TILE_SIZE;
    const int h = BOARD_ROWS * TILE_SIZE;

    Rectangle outer = { (float)boardOrigin.x - 4, (float)boardOrigin.y - 4,
                        (float)w + 8, (float)h + 8 };
    DrawRectangleRec(outer, (Color){30, 30, 35, 255});

    Rectangle inner = { (float)boardOrigin.x, (float)boardOrigin.y,
                        (float)w, (float)h };
    DrawRectangleRec(inner, (Color){15, 15, 20, 255});
}

static void DrawTile(const Tile *t) {
    Rectangle r = { t->x, t->y, (float)TILE_SIZE, (float)TILE_SIZE };
    DrawRectangle((int)r.x + 1, (int)r.y + 2, TILE_SIZE, TILE_SIZE, (Color){0,0,0,40});
    DrawRectangle((int)r.x, (int)r.y, TILE_SIZE, TILE_SIZE, ColorFromIndex(t->color));
    DrawRectangleLinesEx(r, 1.0f, (Color){255,255,255,40});

    char buf[8];
    snprintf(buf, sizeof(buf), "v%d", t->variety);
    DrawText(buf, (int)r.x + 4, (int)r.y + 4, 8, (Color){255,255,255,200});
}

static void DrawCursorAndHighlight(void) {
    // Cursor (current selection) – thin cyan outline
    Rectangle selR = TileRectPx(selGX, selGY);
    DrawRectangleLinesEx(selR, 2.0f, (Color){  0, 220, 255, 200});

    // Highlighted (first chosen tile) – yellow thick outline + translucent fill
    if (hasHighlight) {
        Rectangle hiR = TileRectPx(hiGX, hiGY);
        DrawRectangleRec(hiR, (Color){255, 255,   0, 40});
        DrawRectangleLinesEx(hiR, 3.0f, (Color){255, 215,   0, 255});
    }
}

static void BoardDraw(void) {
    DrawBoardBackdrop();

    // Optional subtle grid lines
    for (int i = 0; i <= BOARD_COLS; ++i) {
        int x = boardOrigin.x + i * TILE_SIZE;
        DrawLine(x, boardOrigin.y, x, boardOrigin.y + BOARD_ROWS * TILE_SIZE, (Color){255,255,255,15});
    }
    for (int j = 0; j <= BOARD_ROWS; ++j) {
        int y = boardOrigin.y + j * TILE_SIZE;
        DrawLine(boardOrigin.x, y, boardOrigin.x + BOARD_COLS * TILE_SIZE, y, (Color){255,255,255,15});
    }

    for (int gy = 1; gy <= BOARD_ROWS; ++gy) {
        for (int gx = 1; gx <= BOARD_COLS; ++gx) {
            DrawTile(TileAt(gx, gy));
        }
    }

    DrawCursorAndHighlight();
}

// ----------------- Input handling (swap1 behavior) -----------------
static void HandleMovement(void) {
    // Move selection with arrow keys (single-step on key press)
    if (IsKeyPressed(KEY_UP) && selGY > 1) selGY--;
    if (IsKeyPressed(KEY_DOWN) && selGY < BOARD_ROWS) selGY++;
    if (IsKeyPressed(KEY_LEFT) && selGX > 1) selGX--;
    if (IsKeyPressed(KEY_RIGHT) && selGX < BOARD_COLS) selGX++;
}

static void SwapTilesAt(int ax, int ay, int bx, int by) {
    // Swap the two tiles in the board array (by value), then fix their coords.
    if (ax == bx && ay == by) return;

    int iax = ax - 1, iay = ay - 1;
    int ibx = bx - 1, iby = by - 1;

    Tile tmp = board.tiles[iay][iax];
    board.tiles[iay][iax] = board.tiles[iby][ibx];
    board.tiles[iby][ibx] = tmp;

    // Update grid coords and pixel positions to match the NEW array positions
    Tile *tA = &board.tiles[iay][iax];
    Tile *tB = &board.tiles[iby][ibx];

    tA->gridX = ax; tA->gridY = ay; UpdateTileXY(tA);
    tB->gridX = bx; tB->gridY = by; UpdateTileXY(tB);
}

static void HandleSelectionAndSwap(void) {
    // Enter/Return selects or swaps
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (!hasHighlight) {
            // First selection
            hasHighlight = true;
            hiGX = selGX;
            hiGY = selGY;
        } else {
            // Second selection -> swap highlighted with current selection
            int aGX = selGX, aGY = selGY;
            int bGX = hiGX, bGY = hiGY;

            SwapTilesAt(aGX, aGY, bGX, bGY);

            // Mirror Lua behavior: after swap, highlighted off and "selected" moves to tile2
            hasHighlight = false;
            selGX = aGX;
            selGY = aGY;
        }
    }

    // Escape cancels highlight
    if (IsKeyPressed(KEY_ESCAPE)) {
        hasHighlight = false;
    }
}

// ----------------- App lifecycle -----------------
static void InitApp(void) {
    Tween_InitSystem(); // ready for next stages
    BoardInit();
}

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_W, WINDOW_H, "Match-3 — swap1: The Static Swap");
    SetTargetFPS(60);

    target = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    InitApp();

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        Tween_UpdateAll(dt); // no tweens in this stage, harmless

        // Input
        HandleMovement();
        HandleSelectionAndSwap();

        // --- Draw to virtual backbuffer ---
        BeginTextureMode(target);
            ClearBackground((Color){18, 20, 24, 255});

            DrawText("swap1 — Static Swap (Arrows move, Enter selects/swaps, Esc cancels)",
                     6, 6, 8, (Color){200,225,255,255});

            BoardDraw();
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

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}

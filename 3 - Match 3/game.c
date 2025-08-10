#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "game.h"

#define TWEEN_IMPL  
#include "tween.h"

// --- Virtual screen / push-like scaling ---
static RenderTexture2D gTarget;     // offscreen framebuffer at virtual size
static Rectangle       gSrcRect;    // source rect for DrawTexturePro (flipped Y)
static Rectangle       gDstRect;    // destination rect on the real window (letterboxed)
static float           gScale = 1.0f;

// Recompute destination rectangle based on current window size.
// Keeps aspect ratio (letterboxing), uses nearest/point sampling for crisp pixels.
static void RecomputeVirtualDst(void) {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    // Min scale that fits the whole virtual framebuffer
    float sx = (float)sw / (float)VIRTUAL_W;
    float sy = (float)sh / (float)VIRTUAL_H;
    gScale = (sx < sy) ? sx : sy;

    float dw = (float)VIRTUAL_W * gScale;
    float dh = (float)VIRTUAL_H * gScale;

    gDstRect = (Rectangle){
        .x = (sw - dw) * 0.5f,
        .y = (sh - dh) * 0.5f,
        .width  = dw,
        .height = dh
    };

    // raylib render textures have their Y flipped; use negative height in src
    gSrcRect = (Rectangle){ 0.0f, 0.0f, (float)VIRTUAL_W, -(float)VIRTUAL_H };
}

// Optional: convert screen coordinates to virtual coordinates (useful if you add mouse controls).
static Vector2 ScreenToVirtual(Vector2 p) {
    // If outside the letterboxed area, return (-1, -1)
    if (p.x < gDstRect.x || p.y < gDstRect.y ||
        p.x > gDstRect.x + gDstRect.width ||
        p.y > gDstRect.y + gDstRect.height) {
        return (Vector2){ -1.0f, -1.0f };
    }
    float vx = (p.x - gDstRect.x) / gScale;
    float vy = (p.y - gDstRect.y) / gScale;
    return (Vector2){ vx, vy };
}


// ============================
// Assets, Tileset, Utilities
// ============================

typedef struct TileSet {
    Texture2D tiles;          // match3.png
    Texture2D background;     // background.png (wide scrolling)
    Font fontMedium;          // UI font
    Font fontLarge;           // Banner font
    Rectangle src[TILE_COLORS][TILE_VARIETIES]; // atlas rects
    bool loaded;
} TileSet;

typedef struct Assets {
    Sound s_select, s_error, s_match, s_clock, s_next_level, s_game_over;
    Music music;
    bool musicPlaying;
} Assets;

static TileSet gTS;
static Assets  G;

// Build atlas rects like Lua's GenerateTileQuads (9 rows, 2 blocks × 6 columns)
static void BuildQuads(TileSet *ts) {
    int y = 0, counter = 0;
    for (int row = 0; row < 9; ++row) {
        for (int block = 0; block < 2; ++block) {
            for (int col = 0; col < TILE_VARIETIES; ++col) {
                ts->src[counter][col] = (Rectangle){ (float)(col * TILE_SIZE), (float)y, TILE_SIZE, TILE_SIZE };
            }
            counter++;
        }
        y += TILE_SIZE;
    }
}

static bool TileSet_Load(TileSet *ts) {
    memset(ts, 0, sizeof(*ts));
    ts->tiles = LoadTexture("res/graphics/match3.png");
    ts->background = LoadTexture("res/graphics/background.png");
    ts->fontMedium = LoadFont("res/fonts/font.ttf");
    ts->fontLarge  = LoadFont("res/fonts/font.ttf");
    if (ts->tiles.id == 0 || ts->background.id == 0 ||
        ts->fontMedium.baseSize == 0 || ts->fontLarge.baseSize == 0) {
        if (ts->fontLarge.baseSize)  UnloadFont(ts->fontLarge);
        if (ts->fontMedium.baseSize) UnloadFont(ts->fontMedium);
        if (ts->background.id)       UnloadTexture(ts->background);
        if (ts->tiles.id)            UnloadTexture(ts->tiles);
        return false;
    }
    BuildQuads(ts);
    ts->loaded = true;
    return true;
}

static void TileSet_Unload(TileSet *ts) {
    if (!ts || !ts->loaded) return;
    UnloadFont(ts->fontLarge);
    UnloadFont(ts->fontMedium);
    UnloadTexture(ts->background);
    UnloadTexture(ts->tiles);
    memset(ts, 0, sizeof(*ts));
}

static inline Rectangle SRC(int color, int variety) { // 1-based
    return gTS.src[color - 1][variety - 1];
}

static void DrawTileWithShadow(int color, int variety, int x, int y, int offx, int offy) {
    Color shadow = (Color){34, 32, 52, 255};
    Vector2 p1 = { (float)(x + offx + 2), (float)(y + offy + 2) };
    DrawTextureRec(gTS.tiles, SRC(color, variety), p1, shadow);
    Vector2 p2 = { (float)(x + offx), (float)(y + offy) };
    DrawTextureRec(gTS.tiles, SRC(color, variety), p2, WHITE);
}

static bool LoadAssets(void) {
    InitAudioDevice();
    G.s_select     = LoadSound("res/sounds/select.wav");
    G.s_error      = LoadSound("res/sounds/error.wav");
    G.s_match      = LoadSound("res/sounds/match.wav");
    G.s_clock      = LoadSound("res/sounds/clock.wav");
    G.s_next_level = LoadSound("res/sounds/next-level.wav");
    G.s_game_over  = LoadSound("res/sounds/game-over.wav");
    G.music        = LoadMusicStream("res/sounds/music2.mp3");
    if (G.music.stream.buffer == NULL) return false;
    SetMusicVolume(G.music, 1.0f);
    PlayMusicStream(G.music);
    G.musicPlaying = true;
    return true;
}

static void UnloadAssets(void) {
    StopMusicStream(G.music);
    UnloadMusicStream(G.music);
    UnloadSound(G.s_select);
    UnloadSound(G.s_error);
    UnloadSound(G.s_match);
    UnloadSound(G.s_clock);
    UnloadSound(G.s_next_level);
    UnloadSound(G.s_game_over);
    CloseAudioDevice();
}

static inline void Assets_Update(void) {
    if (G.musicPlaying) UpdateMusicStream(G.music);
}

static inline void SND_Select(void){ PlaySound(G.s_select); }
static inline void SND_Error(void) { PlaySound(G.s_error); }
static inline void SND_Match(void) { StopSound(G.s_match); PlaySound(G.s_match); }
static inline void SND_Clock(void) { PlaySound(G.s_clock); }
static inline void SND_Next(void)  { PlaySound(G.s_next_level); }
static inline void SND_GameOver(void){ PlaySound(G.s_game_over); }

static float gBgScrollX = 0.0f;
static const float BG_SCROLL_SPEED = 80.0f; // px/s, matches Lua
static void DrawBackground(void) {
    Vector2 pos = { -gBgScrollX, 0 };
    DrawTextureV(gTS.background, pos, WHITE);
    if (gTS.background.width - (int)gBgScrollX < VIRTUAL_W) {
        Vector2 pos2 = { -gBgScrollX + (float)gTS.background.width, 0 };
        DrawTextureV(gTS.background, pos2, WHITE);
    }
}

// ============================
// Board & Tile
// ============================

struct Tile {
    int color;      // 1..18
    int variety;    // 1..6
    int gridX;      // 1..8
    int gridY;      // 1..8
    float x, y;     // pixel pos (top-left), tween target is ((gridX-1)*TILE_SIZE, (gridY-1)*TILE_SIZE)
};

typedef struct MatchResult {
    bool any;
    int matchedCount;
    bool mark[BOARD_ROWS][BOARD_COLS];
} MatchResult;

struct Board {
    Tile *cells[BOARD_ROWS][BOARD_COLS]; // [y][x], NULL means empty
    int offsetX, offsetY;                // top-left pixel of board area
};

// Random helpers
static inline int irand(int lo, int hi) { // inclusive
    return lo + (rand() % (hi - lo + 1));
}

// Create tile (malloc)
static Tile* MakeTile(int gx, int gy) {
    Tile *t = (Tile*)malloc(sizeof(Tile));
    t->gridX = gx; t->gridY = gy;
    t->color = irand(1, TILE_COLORS);
    t->variety = irand(1, TILE_VARIETIES);
    t->x = (float)((gx - 1) * TILE_SIZE);
    t->y = (float)((gy - 1) * TILE_SIZE);
    return t;
}

static void FreeTile(Tile *t) { if (t) free(t); }

// Detect 3+ runs; guarded so it won't crash on accidental NULLs.
static MatchResult Board_CalcMatches(const Board *b) {
    MatchResult r; memset(&r, 0, sizeof(r));

    // Horizontal
    for (int y = 0; y < BOARD_ROWS; ++y) {
        int match = 0, color = -1;
        for (int x = 0; x < BOARD_COLS; ++x) {
            Tile *t = b->cells[y][x];
            int c = t ? t->color : -2;
            if (x == 0) { color = c; match = (c >= 0); }
            else if (c == color && c >= 0) match++;
            else {
                if (match >= 3) {
                    for (int k = 0; k < match; ++k)
                        if (!r.mark[y][x-1-k]) { r.mark[y][x-1-k] = true; r.matchedCount++; }
                    r.any = true;
                }
                color = c;
                match = (c >= 0);
            }
        }
        if (match >= 3) {
            for (int k = 0; k < match; ++k)
                if (!r.mark[y][BOARD_COLS-1-k]) { r.mark[y][BOARD_COLS-1-k] = true; r.matchedCount++; }
            r.any = true;
        }
    }

    // Vertical
    for (int x = 0; x < BOARD_COLS; ++x) {
        int match = 0, color = -1;
        for (int y = 0; y < BOARD_ROWS; ++y) {
            Tile *t = b->cells[y][x];
            int c = t ? t->color : -2;
            if (y == 0) { color = c; match = (c >= 0); }
            else if (c == color && c >= 0) match++;
            else {
                if (match >= 3) {
                    for (int k = 0; k < match; ++k)
                        if (!r.mark[y-1-k][x]) { r.mark[y-1-k][x] = true; r.matchedCount++; }
                    r.any = true;
                }
                color = c;
                match = (c >= 0);
            }
        }
        if (match >= 3) {
            for (int k = 0; k < match; ++k)
                if (!r.mark[BOARD_ROWS-1-k][x]) { r.mark[BOARD_ROWS-1-k][x] = true; r.matchedCount++; }
            r.any = true;
        }
    }

    return r;
}

// Remove marked tiles; return number removed
static int Board_RemoveMatches(Board *b, const MatchResult *mr) {
    int removed = 0;
    for (int y = 0; y < BOARD_ROWS; ++y)
        for (int x = 0; x < BOARD_COLS; ++x)
            if (mr->mark[y][x]) {
                FreeTile(b->cells[y][x]);
                b->cells[y][x] = NULL;
                removed++;
            }
    return removed;
}

// Compact columns (bottom-up); create tweens for tiles that fall; return number of holes filled
static int Board_Fall(Board *b) {
    int holes = 0;
    for (int x = 0; x < BOARD_COLS; ++x) {
        int writeY = BOARD_ROWS - 1;
        for (int y = BOARD_ROWS - 1; y >= 0; --y) {
            Tile *t = b->cells[y][x];
            if (t) {
                if (writeY != y) {
                    b->cells[writeY][x] = t;
                    b->cells[y][x] = NULL;
                    t->gridY = writeY + 1;
                    float ty = (float)((t->gridY - 1) * TILE_SIZE);
                    Tween *tw = Tween_Create(0.1f + 0.05f * (float)(writeY - y));
                    Tween_Add(tw, &t->y, ty);
                    Tween_SetEase(tw, Tween_EaseInQuad);
                    Tween_Start(tw);
                }
                writeY--;
            }
        }
        // count holes in this column
        for (int y = writeY; y >= 0; --y) {
            holes++;
        }
    }
    return holes;
}

// Refill columns from top with new tiles (spawn above then tween down)
static void Board_Refill(Board *b) {
    for (int x = 0; x < BOARD_COLS; ++x) {
        int top = 0;
        while (top < BOARD_ROWS && b->cells[top][x]) top++;
        for (int y = top - 1; y >= 0; --y) {
            int gy = y + 1;
            Tile *t = MakeTile(x + 1, 1);
            t->y = - (float)((top - y) * TILE_SIZE); // spawn above
            t->gridX = x + 1;
            t->gridY = gy;
            float ty = (float)((gy - 1) * TILE_SIZE);
            b->cells[y][x] = t;
            Tween *tw = Tween_Create(0.1f + 0.04f * (float)(top - y));
            Tween_Add(tw, &t->y, ty);
            Tween_SetEase(tw, Tween_EaseInQuad);
            Tween_Start(tw);
        }
    }
}

// Fill initial board without immediate matches (simple retry)
static void Board_FillFresh(Board *b) {
    memset(b->cells, 0, sizeof(b->cells));
    for (int y = 0; y < BOARD_ROWS; ++y) {
        for (int x = 0; x < BOARD_COLS; ++x) {
            int tries = 0;
            do {
                if (b->cells[y][x]) { FreeTile(b->cells[y][x]); b->cells[y][x] = NULL; }
                b->cells[y][x] = MakeTile(x+1, y+1);
                tries++;
                // avoid immediate horizontal
                if (x >= 2) {
                    Tile *a = b->cells[y][x-1], *b0 = b->cells[y][x-2];
                    if (a && b0 && a->color == b0->color && b->cells[y][x]->color == a->color) continue;
                }
                // avoid immediate vertical
                if (y >= 2) {
                    Tile *a = b->cells[y-1][x], *b0 = b->cells[y-2][x];
                    if (a && b0 && a->color == b0->color && b->cells[y][x]->color == a->color) continue;
                }
                break;
            } while (tries < 10);
        }
    }
}

static void Board_Draw(const Board *b) {
    for (int y = 0; y < BOARD_ROWS; ++y) {
        for (int x = 0; x < BOARD_COLS; ++x) {
            Tile *t = b->cells[y][x];
            if (!t) continue;
            DrawTileWithShadow(t->color, t->variety,
                               (x * TILE_SIZE), (y * TILE_SIZE),
                               b->offsetX, b->offsetY);
        }
    }
}

// ============================
// Tiny State Machine (single TU)
// ============================

static void SM_Change(StateMachine *sm, State *next, void *param) {
    if (sm->current && sm->current->exit) sm->current->exit(sm->current);
    sm->current = next;
    if (sm->current && sm->current->enter) sm->current->enter(sm->current, param);
}

static void SM_Init(StateMachine *sm, State *initial) {
    sm->current = NULL;
    SM_Change(sm, initial, NULL);
}

// ============================
// Timer_Every helper
// ============================

typedef struct EveryCtx {
    float interval;
    void (*fn)(void*);
    void *ud;
    bool *alive;
} EveryCtx;

static void Every_Callback(void *ud) {
    EveryCtx *ctx = (EveryCtx*)ud;
    if (!ctx->alive || *ctx->alive) {
        ctx->fn(ctx->ud);
        Timer_After(ctx->interval, Every_Callback, ctx);
        return;
    }
    free(ctx);
}

static void Timer_Every(float interval, void (*fn)(void*), void *ud, bool *aliveFlag) {
    EveryCtx *ctx = (EveryCtx*)malloc(sizeof(EveryCtx));
    ctx->interval = interval; ctx->fn = fn; ctx->ud = ud; ctx->alive = aliveFlag;
    Timer_After(interval, Every_Callback, ctx);
}

// ============================
// Game States (Start, Begin, Play, Over)
// ============================

typedef struct StartData {
    int item;        // 1: Start, 2: Quit
    bool locked;
    float fade;      // 0..1 white fade
} StartData;

typedef struct BeginData {
    int level;
    float labelY;
    Board board;
} BeginData;

typedef struct PlayData {
    int level, score, goal, timer;
    Board board;
    int curGX, curGY;          // cursor grid (1..8)
    bool inputEnabled;
    bool blinking, blinkAlive;
    bool swapping, resolving, swapLogicalDone;
    int aGX, aGY, bGX, bGY;    // swap endpoints (1-based)
    int runningTweens;         // count of in-flight tweens (swap/fall/refill)
    int lastMatches;           // for scoring feedback
    // --- selection (two-step swap) ---
    bool hasSelection;
    int  selGX, selGY;   // 1-based grid coords of the selected tile
} PlayData;

typedef struct OverData {
    int score;
} OverData;

// Globals
State gStart, gBegin, gPlay, gOver;
StateMachine gSM;

// --- Forward helpers that need the above typedefs ---
static void Start_ChangeToBegin(void *ud);
static void Begin_ToPlayAfterSlide(void *ud);
static void Begin_AfterDelay(void *ud);
static void Play_SetResolvingTrue(void *ud);
static void Play_EnableInput(void *ud);

// ---------- Start ----------
static void Start_enter(State *s, void *p) { (void)p;
    StartData *st = (StartData*)s->data;
    st->item = 1; st->locked = false; st->fade = 0.0f;
}
static void Start_exit(State *s){ (void)s; }
static void Start_update(State *s, float dt) {
    (void)dt;
    StartData *st = (StartData*)s->data;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) { st->item = (st->item==1)?2:1; SND_Select(); }
    if (!st->locked && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
        if (st->item == 1) {
            st->locked = true;
            Tween *tw = Tween_Create(1.0f);
            Tween_Add(tw, &st->fade, 1.0f);
            Tween_Start(tw);
            Timer_After(1.05f, Start_ChangeToBegin, NULL); // C function (no lambda)
        } else {
            CloseWindow();
        }
    }
}
static void Start_render(State *s) {
    StartData *st = (StartData*)s->data;
    DrawBackground();
    DrawTextEx(gTS.fontLarge, "MATCH 3", (Vector2){ 162, 60 }, gTS.fontLarge.baseSize, 0, WHITE);
    const char *opt1 = st->item==1 ? "> START" : "  START";
    const char *opt2 = st->item==2 ? "> QUIT " : "  QUIT ";
    DrawTextEx(gTS.fontMedium, opt1, (Vector2){ 210, 140 }, gTS.fontMedium.baseSize, 0, WHITE);
    DrawTextEx(gTS.fontMedium, opt2, (Vector2){ 210, 170 }, gTS.fontMedium.baseSize, 0, WHITE);
    if (st->fade > 0.0f) {
        DrawRectangle(0,0,VIRTUAL_W,VIRTUAL_H, (Color){ 255,255,255,(unsigned char)(st->fade*255) });
    }
}

// ---------- Begin ----------
static void Begin_enter(State *s, void *p) {
    BeginData *bg = (BeginData*)s->data;
    bg->level = p ? *(int*)p : 1;
    bg->labelY = -30.0f;
    bg->board.offsetX = 64; bg->board.offsetY = 16;
    Board_FillFresh(&bg->board);

    // Slide label down (0.25s), wait (1.0s), then slide out (0.25s) → Play
    Tween *t1 = Tween_Create(0.25f); Tween_Add(t1, &bg->labelY, 110.0f); Tween_Start(t1);
    Timer_After(1.0f, Begin_AfterDelay, bg);
}
static void Begin_exit(State *s){ (void)s; }
static void Begin_update(State *s, float dt){ (void)s; (void)dt; }
static void Begin_render(State *s){
    BeginData *bg = (BeginData*)s->data;
    DrawBackground();
    // board preview
    DrawRectangle(BOARD_OFFSET_X - 4, BOARD_OFFSET_Y - 4,
                  BOARD_COLS*TILE_SIZE + 8, BOARD_ROWS*TILE_SIZE + 8, (Color){0,0,0,80});
    bg->board.offsetX = BOARD_OFFSET_X;
    bg->board.offsetY = BOARD_OFFSET_Y;
    Board_Draw(&bg->board);

    // level banner
    DrawRectangle(0, (int)bg->labelY - 8, VIRTUAL_W, 48, (Color){95,205,228,200});
    DrawTextEx(gTS.fontLarge, TextFormat("Level %d", bg->level),
               (Vector2){ 180, bg->labelY }, gTS.fontLarge.baseSize, 0, WHITE);
}

// After delay: slide label out and move to Play
static void Begin_AfterDelay(void *ud) {
    BeginData *bg = (BeginData*)ud;
    Tween *t3 = Tween_Create(0.25f);
    Tween_Add(t3, &bg->labelY, (float)(VIRTUAL_H + 30));
    Tween_Start(t3);
    Timer_After(0.26f, Begin_ToPlayAfterSlide, bg);
}

static void Begin_ToPlayAfterSlide(void *ud) {
    BeginData *bg = (BeginData*)ud;
    PlayData *ps = (PlayData*)gPlay.data;
    memset(ps, 0, sizeof(*ps));
    ps->level = bg->level; ps->score = 0;
    ps->goal = (int)(ps->level * 1.25f * 1000.0f);
    ps->timer = 60;
    ps->curGX = 1; ps->curGY = 1;
    ps->inputEnabled = true;
    ps->board = bg->board;              // transfer ownership
    ps->hasSelection = false;
    ps->selGX = ps->selGY = 0;

    memset(&bg->board, 0, sizeof(bg->board));
    SM_Change(&gSM, &gPlay, NULL);
}

static void Start_ChangeToBegin(void *ud) { (void)ud;
    int lv = 1; SM_Change(&gSM, &gBegin, &lv);
}

// ---------- Play ----------
static void Play_ToggleHighlight(void *ud) {
    PlayData *ps = (PlayData*)ud;
    ps->blinking = !ps->blinking;
}

static void Play_TickTimer(void *ud) {
    PlayData *ps = (PlayData*)ud;
    if (ps->timer > 0) {
        ps->timer--;
        if (ps->timer <= 5) SND_Clock();
        if (ps->timer == 0) {
            SND_GameOver();
            OverData *ov = (OverData*)gOver.data; ov->score = ps->score;
            SM_Change(&gSM, &gOver, NULL);
        }
    }
}

static void Play_enter(State *s, void *p){ (void)p;
    PlayData *ps = (PlayData*)s->data;
    ps->blinkAlive = true;
    ps->hasSelection = false;  // ensure clean state when entering Play
    ps->selGX = ps->selGY = 0;
    Timer_Every(0.5f, Play_ToggleHighlight, ps, &ps->blinkAlive);
    Timer_Every(1.0f, Play_TickTimer, ps, &ps->blinkAlive);
}


static void Play_exit(State *s){
    PlayData *ps = (PlayData*)s->data;
    ps->blinkAlive = false; // stop repeating timers
}

static void Play_EnableInput(void *ud) {
    ((PlayData*)ud)->inputEnabled = true;
}
static void Play_SetResolvingTrue(void *ud) {
    ((PlayData*)ud)->resolving = true;
}

// Start a swap tween between (aGX,aGY) and (bGX,bGY)
typedef struct SwapCtx {
    PlayData *ps;
    Board *b;
} SwapCtx;

static void OnSwapTweenFinished(void *ud) {
    SwapCtx *ctx = (SwapCtx*)ud;
    PlayData *ps = ctx->ps;

    if (ps->runningTweens > 0) ps->runningTweens--;
    if (ps->runningTweens == 0) {
        if (!ps->swapLogicalDone) {
            int ax = ps->aGX - 1, ay = ps->aGY - 1;
            int bx = ps->bGX - 1, by = ps->bGY - 1;
            Tile *tmp = ctx->b->cells[ay][ax];
            ctx->b->cells[ay][ax] = ctx->b->cells[by][bx];
            ctx->b->cells[by][bx] = tmp;

            Tile *A = ctx->b->cells[ay][ax];
            Tile *B = ctx->b->cells[by][bx];
            A->gridX = ps->aGX; A->gridY = ps->aGY; A->x = (A->gridX-1)*TILE_SIZE; A->y = (A->gridY-1)*TILE_SIZE;
            B->gridX = ps->bGX; B->gridY = ps->bGY; B->x = (B->gridX-1)*TILE_SIZE; B->y = (B->gridY-1)*TILE_SIZE;

            ps->swapLogicalDone = true;
        }
        ps->swapping = false;
        ps->resolving = true;
        free(ctx);
    }
}

static inline int manhattan(int ax, int ay, int bx, int by) {
    return abs(ax - bx) + abs(ay - by);
}

// Start a swap tween explicitly between (aGX,aGY) and (bGX,bGY), 1-based.
static void StartSwapPair(PlayData *ps, Board *b, int aGX, int aGY, int bGX, int bGY) {
    ps->aGX = aGX; ps->aGY = aGY;
    ps->bGX = bGX; ps->bGY = bGY;

    int ax = aGX - 1, ay = aGY - 1;
    int bx = bGX - 1, by = bGY - 1;

    Tile *A = b->cells[ay][ax];
    Tile *B = b->cells[by][bx];
    if (!A || !B) return;

    ps->swapping = true; 
    ps->swapLogicalDone = false; 
    ps->runningTweens = 2; 
    ps->inputEnabled = false;

    float Ax = (float)((bGX - 1) * TILE_SIZE);
    float Ay = (float)((bGY - 1) * TILE_SIZE);
    float Bx = (float)((aGX - 1) * TILE_SIZE);
    float By = (float)((aGY - 1) * TILE_SIZE);

    SwapCtx *ctx = (SwapCtx*)malloc(sizeof(SwapCtx)); 
    ctx->ps = ps; 
    ctx->b  = b;

    Tween *tA = Tween_Create(0.10f);
    Tween_Add(tA, &A->x, Ax); Tween_Add(tA, &A->y, Ay);
    Tween_OnFinish(tA, OnSwapTweenFinished, ctx);
    Tween_Start(tA);

    Tween *tB = Tween_Create(0.10f);
    Tween_Add(tB, &B->x, Bx); Tween_Add(tB, &B->y, By);
    Tween_OnFinish(tB, OnSwapTweenFinished, ctx);
    Tween_Start(tB);

    // Re-enable input shortly after
    Timer_After(0.12f, Play_EnableInput, ps);
}

// Keep the original convenience that swaps cursor ↔ target (used by arrows+Enter logic if needed)
static void StartSwap(PlayData *ps, Board *b, int gx, int gy) {
    StartSwapPair(ps, b, ps->curGX, ps->curGY, gx, gy);
}

// Resolve loop: calculate matches, remove, score, fall, refill, check again
static void Play_ResolveStep(PlayData *ps) {
    Board *b = &ps->board;
    MatchResult mr = Board_CalcMatches(b);
    if (!mr.any) {
        ps->resolving = false;
        return;
    }
    // scoring
    ps->lastMatches = mr.matchedCount;
    ps->score += ps->lastMatches * 50; // simple scoring; mirrors CS50 feel
    SND_Match();

    // remove → fall → refill → schedule next resolve after tweens
    Board_RemoveMatches(b, &mr);
    int holes = Board_Fall(b);
    Board_Refill(b);
    (void)holes;

    // After falls/refill complete (tweens are short), resolve again
    Timer_After(0.18f, Play_SetResolvingTrue, ps);
}

static void Play_update(State *s, float dt) {
    (void)dt;
    PlayData *ps = (PlayData*)s->data;

    // Input (move cursor + selection)
    if (ps->inputEnabled && !ps->swapping) {
        int dx = 0, dy = 0;
        if (IsKeyPressed(KEY_LEFT))  dx = -1;
        if (IsKeyPressed(KEY_RIGHT)) dx = +1;
        if (IsKeyPressed(KEY_UP))    dy = -1;
        if (IsKeyPressed(KEY_DOWN))  dy = +1;

        if (dx || dy) {
            int nx = ps->curGX + dx, ny = ps->curGY + dy;
            if (nx >= 1 && nx <= BOARD_COLS && ny >= 1 && ny <= BOARD_ROWS) {
                ps->curGX = nx; ps->curGY = ny;
                SND_Select();
            }
        }

        // Confirm/selection handling
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            if (!ps->hasSelection) {
                // Step 1: select current tile
                ps->hasSelection = true;
                ps->selGX = ps->curGX;
                ps->selGY = ps->curGY;
                SND_Select();
            } else {
                // Step 2: attempt swap between selected tile and cursor
                if (manhattan(ps->selGX, ps->selGY, ps->curGX, ps->curGY) == 1) {
                    StartSwapPair(ps, &ps->board, ps->selGX, ps->selGY, ps->curGX, ps->curGY);
                    ps->hasSelection = false;   // clear selection after initiating swap
                } else if (ps->selGX == ps->curGX && ps->selGY == ps->curGY) {
                    // Enter on the same tile toggles selection off
                    ps->hasSelection = false;
                    SND_Select();
                } else {
                    // Not adjacent → error beep, keep selection
                    SND_Error();
                }
            }
        }

        // Cancel selection with Esc
        if (ps->hasSelection && IsKeyPressed(KEY_ESCAPE)) {
            ps->hasSelection = false;
            SND_Select();
        }
    }

    // Resolve loop (unchanged)
    if (ps->resolving && !ps->swapping) {
        Play_ResolveStep(ps);
        // Level up if goal reached
        if (ps->score >= ps->goal) {
            SND_Next();
            int nextLv = ps->level + 1;
            ps->blinkAlive = false; // stop timers
            SM_Change(&gSM, &gBegin, &nextLv);
            return;
        }
    }
}


static void Play_render(State *s) {
    PlayData *ps = (PlayData*)s->data;
    DrawBackground();

    // HUD panel (left side)
    DrawRectangle(16,16,186,116,(Color){56,56,56,234});
    DrawTextEx(gTS.fontMedium, TextFormat("Level: %d", ps->level), (Vector2){20,24},  gTS.fontMedium.baseSize, 0, (Color){99,155,255,255});
    DrawTextEx(gTS.fontMedium, TextFormat("Score: %d", ps->score), (Vector2){20,52},  gTS.fontMedium.baseSize, 0, (Color){99,155,255,255});
    DrawTextEx(gTS.fontMedium, TextFormat("Goal : %d", ps->goal),  (Vector2){20,80},  gTS.fontMedium.baseSize, 0, (Color){99,155,255,255});
    DrawTextEx(gTS.fontMedium, TextFormat("Timer: %d", ps->timer), (Vector2){20,108}, gTS.fontMedium.baseSize, 0, (Color){99,155,255,255});

    // Board backdrop and offsets (moved right)
    int bx = BOARD_OFFSET_X - 4;
    int by = BOARD_OFFSET_Y - 4;
    DrawRectangle(bx, by, BOARD_COLS*TILE_SIZE + 8, BOARD_ROWS*TILE_SIZE + 8, (Color){0,0,0,80});

    ps->board.offsetX = BOARD_OFFSET_X;
    ps->board.offsetY = BOARD_OFFSET_Y;
    Board_Draw(&ps->board);

    // Cursor highlight (blinking)
    if (ps->blinking) {
        int rx = ps->board.offsetX + (ps->curGX - 1)*TILE_SIZE - 2;
        int ry = ps->board.offsetY + (ps->curGY - 1)*TILE_SIZE - 2;
        DrawRectangleLinesEx((Rectangle){ rx, ry, TILE_SIZE + 4, TILE_SIZE + 4 }, 2, (Color){255,255,255,220});
    }

    // Selected tile highlight (persistent, distinct style)
    if (ps->hasSelection) {
        int sx = ps->board.offsetX + (ps->selGX - 1)*TILE_SIZE - 3;
        int sy = ps->board.offsetY + (ps->selGY - 1)*TILE_SIZE - 3;
        DrawRectangleLinesEx((Rectangle){ sx, sy, TILE_SIZE + 6, TILE_SIZE + 6 }, 3, (Color){255, 223, 0, 230});
    }
}


// ---------- Over ----------
static void Over_enter(State *s, void *p){
    (void)s; (void)p;  // silence unused parameter warnings
}

static void Over_exit(State *s){ (void)s; }
static void Over_update(State *s, float dt){ (void)s; (void)dt;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        SM_Change(&gSM, &gStart, NULL);
    }
}
static void Over_render(State *s){
    OverData *ov = (OverData*)s->data;
    DrawBackground();
    DrawTextEx(gTS.fontLarge, "GAME OVER", (Vector2){ 145, 90 }, gTS.fontLarge.baseSize, 0, WHITE);
    DrawTextEx(gTS.fontMedium, TextFormat("Score: %d", ov->score), (Vector2){ 185, 140 }, gTS.fontMedium.baseSize, 0, WHITE);
    DrawTextEx(gTS.fontMedium, "Press Enter", (Vector2){ 190, 180 }, gTS.fontMedium.baseSize, 0, WHITE);
}

// ============================
// Game lifecycle
// ============================

static StartData gStartData;
static BeginData gBeginData;
static PlayData  gPlayData;
static OverData  gOverData;

bool Game_Init(void) {
    SetRandomSeed((unsigned int)time(NULL));

    // Create a resizable window (can be any desktop size); we render at virtual size into gTarget.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(VIRTUAL_W * 2, VIRTUAL_H * 2, "Match 3 (C99 + raylib)");
    SetTargetFPS(60);

    // Load tiles/fonts/background first
    if (!TileSet_Load(&gTS)) return false;

    // Ensure crisp pixel art (point filtering) for textures drawn into the virtual target
    SetTextureFilter(gTS.tiles,      TEXTURE_FILTER_POINT);
    SetTextureFilter(gTS.background, TEXTURE_FILTER_POINT);
    // Fonts normally stay as default; you can set point too if you want brutal pixel look:
    // SetTextureFilter(gTS.fontMedium.texture, TEXTURE_FILTER_POINT);
    // SetTextureFilter(gTS.fontLarge.texture,  TEXTURE_FILTER_POINT);

    // Load audio assets
    if (!LoadAssets()) return false;

    // --- NEW: create the virtual render target and set point filtering for crisp upscaling
    gTarget = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(gTarget.texture, TEXTURE_FILTER_POINT);
    RecomputeVirtualDst();   // compute initial letterbox rectangle

    // States
    gStart = (State){ &gStartData, Start_enter, Start_exit, Start_update, Start_render };
    gBegin = (State){ &gBeginData, Begin_enter, Begin_exit, Begin_update, Begin_render };
    gPlay  = (State){ &gPlayData,  Play_enter,  Play_exit,  Play_update,  Play_render  };
    gOver  = (State){ &gOverData,  Over_enter,  Over_exit,  Over_update,  Over_render  };

    SM_Init(&gSM, &gStart);
    return true;
}


void Game_Shutdown(void) {
    // free any leftover board tiles
    for (int y = 0; y < BOARD_ROWS; ++y)
        for (int x = 0; x < BOARD_COLS; ++x) {
            if (gBeginData.board.cells[y][x]) FreeTile(gBeginData.board.cells[y][x]);
            if (gPlayData.board.cells[y][x])  FreeTile(gPlayData.board.cells[y][x]);
        }

    UnloadRenderTexture(gTarget);
    UnloadAssets();
    TileSet_Unload(&gTS);
    CloseWindow();
}

void Game_Update(float dt) {
    // Tween/Timer tick first so logic sees fresh state
    Tween_UpdateAll(dt);
    gBgScrollX += BG_SCROLL_SPEED * dt;
    if (gBgScrollX >= (float)(gTS.background.width - VIRTUAL_W + 51)) gBgScrollX = 0.0f;
    Assets_Update();

    if (gSM.current && gSM.current->update) gSM.current->update(gSM.current, dt);
}

void Game_Render(void) {
    // 1) Draw the scene at virtual resolution into the offscreen render texture.
    BeginTextureMode(gTarget);
        // Clear the virtual framebuffer; your states will draw background etc.
        ClearBackground((Color){ 0, 0, 0, 255 });

        if (gSM.current && gSM.current->render) gSM.current->render(gSM.current);
    EndTextureMode();

    // 2) Now draw that texture to the actual window, letterboxed & scaled.
    BeginDrawing();
        ClearBackground(BLACK);                 // bars around the letterbox
        RecomputeVirtualDst();                  // update on window resize

        DrawTexturePro(
            gTarget.texture,
            gSrcRect,                           // source at virtual size (Y flipped)
            gDstRect,                           // destination letterboxed rect
            (Vector2){ 0, 0 },                  // origin
            0.0f,
            WHITE
        );
    EndDrawing();
}


// ============================
// Entry point
// ============================

int main(void) {
    if (!Game_Init()) return 1;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Game_Update(dt);
        Game_Render();
    }

    Game_Shutdown();
    return 0;
}

// game.c
// Stage 1: Tween demo for CS50 Match-3 (Raylib).
// Build (example):
//   gcc game.c -o game -DTWEEN_IMPL -I. -lraylib -lm
// Place a 24x24 sprite at: res/flappy.png

#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define TWEEN_IMPL
#include "tween.h"

// Virtual resolution (close to CS50 examples)
static const int VIRTUAL_W = 384;
static const int VIRTUAL_H = 216;

// Window size
static const int WINDOW_W = 1280;
static const int WINDOW_H = 720;

// Longest possible tween duration (seconds)
static const float TIMER_MAX = 10.0f;

// How many sprites to display (adjust if needed)
#ifndef BIRD_COUNT
#define BIRD_COUNT 600  // 1000 is fine on many machines; start moderate.
#endif

typedef struct {
    float x;
    float y;
    float opacity;  // 0..255
} Bird;

static Bird birds[BIRD_COUNT];
static Texture2D birdTex;
static RenderTexture2D target;

static void InitDemo(void) {
    // Load sprite (24x24 recommended)
    birdTex = LoadTexture("res/graphics/flappy.png");

    // Create birds: start at x=0, random y, random duration, tween to endX
    const float endX = (float)VIRTUAL_W - (float)birdTex.width;

    srand((unsigned)time(NULL));
    Tween_InitSystem();

    for (int i = 0; i < BIRD_COUNT; ++i) {
        birds[i].x = 0.0f;
        birds[i].y = (float)(rand() % (VIRTUAL_H - birdTex.height));
        birds[i].opacity = 0.0f;

        // Random duration in [0.5, TIMER_MAX]
        float duration = 0.5f + ((float)rand() / (float)RAND_MAX) * (TIMER_MAX - 0.5f);

        Tween *tw = Tween_Create(duration);
        // Move x to endX, fade opacity to 255
        Tween_Add(tw, &birds[i].x, endX);
        Tween_Add(tw, &birds[i].opacity, 255.0f);
        // Linear for now; easings will come later
        Tween_Start(tw);
    }
}

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_W, WINDOW_H, "Match-3 — Stage 1: Tween");
    SetTargetFPS(60);

    // Backbuffer for crisp pixel scaling
    target = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    InitDemo();

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();

        // Update all tweens
        Tween_UpdateAll(dt);

        // --- Draw to virtual backbuffer ---
        BeginTextureMode(target);
            ClearBackground(BLACK);

            // Draw birds with per-sprite alpha
            for (int i = 0; i < BIRD_COUNT; ++i) {
                unsigned char a = (unsigned char)fminf(fmaxf(birds[i].opacity, 0.0f), 255.0f);
                Color tint = (Color){255, 255, 255, a};
                DrawTexture(birdTex, (int)birds[i].x, (int)birds[i].y, tint);
            }

            // Simple label
            DrawText("Stage 1: Tween (linear) — Press ESC to quit",
                     6, 6, 8, (Color){200, 225, 255, 255});
        EndTextureMode();

        // --- Present with integer-ish scaling ---
        BeginDrawing();
            ClearBackground(RAYWHITE);

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

    // Cleanup
    UnloadTexture(birdTex);
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}

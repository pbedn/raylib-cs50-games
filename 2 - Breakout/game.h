#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "stdbool.h"
#include <iso646.h>
#include <float.h>
#include <stddef.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include "raylib.h"
#include "raymath.h"

/* HELPERS */
#define MAX(a, b) ((a)>(b)? (a) : (b))
#define MIN(a, b) ((a)<(b)? (a) : (b))

Font defaultFont;
void drawText(const char *text, int x, int y, float fontSize, Color color) {
    DrawTextEx(defaultFont, text, (Vector2){x, y}, fontSize, 2, color);
}

void UpdateDrawFrame(RenderTexture2D target);
void GameLogic(float deltaTime);
void DrawFPSCustom(void);
void DrawGame(void);
void DrawTitle(void);

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
} GameState;

extern GameState currentState;

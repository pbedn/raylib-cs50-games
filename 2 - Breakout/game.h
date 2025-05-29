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

/* GAME */
void UpdateDrawFrame(RenderTexture2D target);
void GameLogic(float deltaTime);
void DrawFPSCustom(void);
void DrawGame(void);
void UpdateStartMenu(void);
void DrawStartMenu(void);

typedef enum {
    STATE_START,
    STATE_PLAY,
} GameState;

extern GameState currentState;

typedef struct {
    int highlighted;          // 1 = START, 2 = HIGH SCORES
} StartMenu;

extern StartMenu startMenu; 

typedef struct {
    float x, y;
    float dx;
    int width, height;
    int skin;  // 1 to 4
    int size;  // 1 to 4
} Paddle;

extern Paddle playerPaddle;

void InitPaddleQuads();
void InitPaddle(Paddle *p);
void UpdatePaddle(Paddle *p, float dt);
void DrawPaddle(Paddle *p);

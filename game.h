#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "stdbool.h"
#include <iso646.h>
#include <float.h>
#include <stddef.h>
#include <unistd.h>
#include <math.h>

#include "raylib.h"
#include "raymath.h"

void UpdateDrawFrame(RenderTexture2D target);
void GameLogic(float deltaTime);
void DrawGame(void);
void DrawTitle(void);
void ScrollingBackground(float deltaTime);
void ResetGame();

typedef enum {
    STATE_TITLE,
    STATE_COUNTDOWN,
    STATE_PLAY,
    STATE_SCORE
} GameState;

extern GameState currentState;
extern int countdownTime;

typedef struct {
    Texture2D image;
    int width;
    int height;
    // position bird in the middle of the screen
    int x;
    int y;
    // Y velocity; gravity
    float dy;
} Bird;

void InitBird(Bird *bird);
void UpdateBird(float dt, Bird *bird);
void DrawBird(Bird *bird);

typedef struct {
    Texture2D image;
    int scroll;
    int width;
    int height;
    float x;
    float y;
    int flipped;
} Pipe;

bool CollideBird(Bird *bird, Pipe *pipe);
Pipe InitPipe(int y, int flipped);
void UpdatePipe(float dt, Pipe *pipe);
void DrawPipe(Pipe *pipe);

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

Pipe InitPipe(int y, int flipped);
void UpdatePipe(float dt, Pipe *pipe);
void DrawPipe(Pipe *pipe);

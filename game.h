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
} Bird;

void DrawBird(Bird bird);

#ifndef GAME_H
#define GAME_H

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
    STATE_SERVE,
    STATE_GAME_OVER,
} GameState;

typedef struct {
    int highlighted; // 1 = START, 2 = HIGH SCORES
} StartMenu;


#define PADDLE_SPEED 200.0f
#define PADDLE_SKINS 4
#define PADDLE_SIZES 4

typedef struct {
    float x, y;
    float dx;
    int width, height;
    int skin;  // 1 to 4
    int size;  // 1 to 4
} Paddle;

void InitPaddleQuads(void);
void InitPaddle(Paddle *p);
void UpdatePaddle(Paddle *p, float dt);
void DrawPaddle(Paddle *p);


typedef struct {
    float x, y;
    float dx, dy;
    int width, height;
    int skin;  // 0 to 6
} Ball;

void InitBallQuads(void);
void InitBall(Ball *b);
void UpdateBall(Ball *b, float dt);
void DrawBall(Ball *b);

#define BRICK_WIDTH 32
#define BRICK_HEIGHT 16
#define MAX_BRICKS 100
#define BRICK_QUAD_COUNT 21

typedef struct {
    float x, y;
    float width, height;
    bool inPlay;
    int color;
    int tier;
    int spriteIndex;
} Brick;

void InitBrickQuads(void);
void InitBricks(void);
void DrawBricks(void);

void ServeState(float dt);
void GameOverState(void);
void DrawHealth(void);
void DrawServe(void);
void DrawGameOver(void);

void InitGameState(void);

void HandleBallPaddleCollision(Ball *ball, Paddle *paddle);
void HandleBallBrickCollision(Ball *ball, Brick *brick);

#endif // GAME_H

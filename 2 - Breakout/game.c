#include "game.h"
#include "raylib.h"
#include <stdio.h>

const int screenWidth = 1280;
const int screenHeight = 720;

int gameScreenWidth = 432;
int gameScreenHeight = 243;

bool isPaused = false;

GameState currentState = STATE_START;

StartMenu startMenu = { .highlighted = 1 };

Paddle playerPaddle;

Color blueColor = {103, 255, 255, 255};

Rectangle paddleQuads[PADDLE_SKINS * PADDLE_SIZES];
Rectangle ballQuads[7];
Ball ball;
int brickCount;
Brick bricks[MAX_BRICKS];
Rectangle brickQuads[BRICK_QUAD_COUNT];

int health;
int score;
int level;

// Resources
Texture2D backgroundTexture;
Texture2D mainTexture;
Texture2D arrowsTexture;
Texture2D heartsTexture;
Texture2D particleTexture;

Font smallFont;
Font mediumFont;
Font largeFont;

Sound paddleHitSound;
Sound scoreSound;
Sound wallHitSound;
Sound confirmSound;
Sound selectSound;
Sound noSelectSound;
Sound brickHit1Sound;
Sound brickHit2Sound;
Sound hurtSound;
Sound victorySound;
Sound recoverSound;
Sound highScoreSound;
Sound pauseSound;
Music music;

int main() {
    SetTraceLogLevel(LOG_WARNING);

    /* Initialization: Set up the window and load game resources. */
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Breakout");

    InitAudioDevice();

    // Load Fonts
    smallFont = LoadFontEx("res/fonts/font.ttf", 8, 0, 0);
    mediumFont = LoadFontEx("res/fonts/font.ttf", 16, 0, 0);
    largeFont = LoadFontEx("res/fonts/font.ttf", 32, 0, 0);

    // Load Graphics
    backgroundTexture = LoadTexture("res/graphics/background.png");
    mainTexture = LoadTexture("res/graphics/breakout.png");
    arrowsTexture = LoadTexture("res/graphics/arrows.png");
    heartsTexture = LoadTexture("res/graphics/hearts.png");
    particleTexture = LoadTexture("res/graphics/particle.png");

    // Load Sounds / Music
    paddleHitSound = LoadSound("res/sounds/paddle_hit.wav");
    scoreSound = LoadSound("res/sounds/score.wav");
    wallHitSound = LoadSound("res/sounds/wall_hit.wav");
    confirmSound = LoadSound("res/sounds/confirm.wav");
    selectSound = LoadSound("res/sounds/select.wav");
    noSelectSound = LoadSound("res/sounds/no-select.wav");
    brickHit1Sound = LoadSound("res/sounds/brick-hit-1.wav");
    brickHit2Sound = LoadSound("res/sounds/brick-hit-2.wav");
    hurtSound = LoadSound("res/sounds/hurt.wav");
    victorySound = LoadSound("res/sounds/victory.wav");
    recoverSound = LoadSound("res/sounds/recover.wav");
    highScoreSound = LoadSound("res/sounds/high_score.wav");
    pauseSound = LoadSound("res/sounds/pause.wav");
    music = LoadMusicStream("res/sounds/music.wav");

    // Start music
    SetMusicVolume(music, 0.25f);
    PlayMusicStream(music);

    // Render texture initialization, used to hold the rendering result so we can easily resize it
    RenderTexture2D target = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);  // Texture scale filter to use

    InitGameState();
    InitPaddleQuads();
    InitPaddle(&playerPaddle);
    InitBallQuads();
    InitBall(&ball);
    InitBrickQuads();
    InitBricks();

    srand(time(NULL));
    SetTargetFPS(60);

    while (!WindowShouldClose()) {

        UpdateDrawFrame(target);
    }

    /* De-Initialization: Clean up resources and close the window. */
    // Load Fonts
    UnloadFont(smallFont);
    UnloadFont(mediumFont);
    UnloadFont(largeFont);

    // Load Graphics
    UnloadTexture(backgroundTexture);
    UnloadTexture(mainTexture);
    UnloadTexture(arrowsTexture);
    UnloadTexture(heartsTexture);
    UnloadTexture(particleTexture);

    // Load Sounds / Music
    UnloadSound(paddleHitSound);
    UnloadSound(scoreSound);
    UnloadSound(wallHitSound);
    UnloadSound(confirmSound);
    UnloadSound(selectSound);
    UnloadSound(noSelectSound);
    UnloadSound(brickHit1Sound);
    UnloadSound(brickHit2Sound);
    UnloadSound(hurtSound);
    UnloadSound(victorySound);
    UnloadSound(recoverSound);
    UnloadSound(highScoreSound);
    UnloadSound(pauseSound);
    UnloadMusicStream(music);

    CloseWindow(); // Close window and OpenGL context

    return 0;
}

void UpdateDrawFrame(RenderTexture2D target)
{
    if (currentState == STATE_PLAY && IsKeyPressed(KEY_SPACE)) {
        isPaused = !isPaused;
        PlaySound(pauseSound);

        if (isPaused) PauseMusicStream(music);
        else ResumeMusicStream(music);
    }

    float deltaTime = GetFrameTime();
    // Compute required framebuffer scaling
    float scale = MIN((float)GetScreenWidth()/gameScreenWidth, (float)GetScreenHeight()/gameScreenHeight);

    UpdateMusicStream(music);

    if (!isPaused) {
        switch (currentState) {
            case STATE_START:
                UpdateStartMenu();
                break;
            case STATE_SERVE:
                ServeState(deltaTime);
                break;
            case STATE_PLAY:
                GameLogic(deltaTime);
                break;
            case STATE_GAME_OVER:
                GameOverState();
                break;
        }
    }

    BeginTextureMode(target);
        ClearBackground(WHITE);
        
        // Background
        Rectangle src = { 0, 0, backgroundTexture.width, backgroundTexture.height };
        Rectangle dst = { 0, 0, gameScreenWidth + 1, gameScreenHeight + 2 };
        DrawTexturePro(backgroundTexture, src, dst, (Vector2){0,0}, 0, WHITE);

        if (currentState == STATE_START)
            DrawStartMenu();
        else if (currentState == STATE_PLAY)
            DrawGame();
        else if (currentState == STATE_SERVE)
            DrawServe();
        else if (currentState == STATE_GAME_OVER)
            DrawGameOver();

        DrawFPSCustom();
    EndTextureMode();

    BeginDrawing();
        ClearBackground(WHITE);

        // Draw render texture to screen, properly scaled
        DrawTexturePro(target.texture, (Rectangle){ 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height },
                       (Rectangle){ (GetScreenWidth() - ((float)gameScreenWidth*scale))*0.5f, (GetScreenHeight() - ((float)gameScreenHeight*scale))*0.5f,
                       (float)gameScreenWidth*scale, (float)gameScreenHeight*scale }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndDrawing();
}

/* UPDATE FUNCTIONS */

void InitGameState()
{
    brickCount = 0;
    level = 1;
    health = 3;
    score = 0;
}

void GameLogic(float dt)
{
    UpdatePaddle(&playerPaddle, dt);
    UpdateBall(&ball, dt);

    if (ball.y >= gameScreenHeight) {
        PlaySound(hurtSound);
        health--;

        if (health == 0) {
            currentState = STATE_GAME_OVER;
        } else {
            currentState = STATE_SERVE;
        }
    }

    Rectangle ballRect = { ball.x, ball.y, ball.width, ball.height };

    // Ball-Paddle Collision
    Rectangle paddleRect = { playerPaddle.x, playerPaddle.y, (float)playerPaddle.width, (float)playerPaddle.height };
    if (CheckCollisionRecs(ballRect, paddleRect)) {
        ball.y = playerPaddle.y - ball.height;
        ball.dy = -ball.dy;

        float paddleCenter = playerPaddle.x + playerPaddle.width / 2.0f;
        float ballCenter = ball.x + ball.width / 2;
        float diff = paddleCenter - ballCenter;

        if (ballCenter < paddleCenter && playerPaddle.dx < 0) {
            ball.dx = -50.0f - 8.0f * fabsf(diff);
        } else if (ballCenter > paddleCenter && playerPaddle.dx > 0) {
            ball.dx = 50.0f + 8.0f * fabsf(diff);
        }

        PlaySound(paddleHitSound);
    }

    // Ball-Brick Collision with Edge Detection
    for (int i = 0; i < brickCount; i++) {
        if (bricks[i].inPlay) {
            Rectangle brickRect = { bricks[i].x, bricks[i].y, bricks[i].width, bricks[i].height };
            if (CheckCollisionRecs(ballRect, brickRect)) {
                bricks[i].inPlay = false;
                PlaySound(brickHit2Sound);
                score += 10;

                if (ball.x + ball.width - 1 < bricks[i].x && ball.dx > 0) {
                    // Hit left side
                    ball.dx = -ball.dx;
                    ball.x = bricks[i].x - ball.width;
                } else if (ball.x + 1 > bricks[i].x + bricks[i].width && ball.dx < 0) {
                    // Hit right side
                    ball.dx = -ball.dx;
                    ball.x = bricks[i].x + bricks[i].width;
                } else if (ball.y < bricks[i].y) {
                    // Hit top of brick, ball going down
                    ball.dy = -ball.dy;
                    ball.y = bricks[i].y - ball.height;
                } else {
                    // Hit bottom of brick, ball going up
                    ball.dy = -ball.dy;
                    ball.y = bricks[i].y + bricks[i].height;
                }

                // slight speed up
                ball.dy *= 1.02f;

                break; // only one brick per frame
            }
        }
    }
}


void UpdateStartMenu()
{
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        startMenu.highlighted = (startMenu.highlighted == 1) ? 2 : 1;
        PlaySound(paddleHitSound);
    }

    if (IsKeyPressed(KEY_ENTER)) {
        PlaySound(confirmSound);
        if (startMenu.highlighted == 1) {
            currentState = STATE_SERVE;
        }
    }
}

void InitPaddleQuads()
{
    int counter = 0;
    for (int i = 0; i < PADDLE_SKINS; i++) {
        int y = 64 + i * 32;

        paddleQuads[counter++] = (Rectangle){ 0,      y, 32, 16 };  // small
        paddleQuads[counter++] = (Rectangle){ 32,     y, 64, 16 };  // medium
        paddleQuads[counter++] = (Rectangle){ 96,     y, 96, 16 };  // large
        paddleQuads[counter++] = (Rectangle){ 0,  y + 16,128, 16 }; // huge
    }
}

void InitPaddle(Paddle *p)
{
    p->x = gameScreenWidth / 2 - 32;
    p->y = gameScreenHeight - 32;
    p->dx = 0;
    p->width = 64;
    p->height = 16;
    p->skin = 1;
    p->size = 2;
}

void UpdatePaddle(Paddle *p, float dt)
{
    if (IsKeyDown(KEY_LEFT)) {
        p->dx = -PADDLE_SPEED;
    } else if (IsKeyDown(KEY_RIGHT)) {
        p->dx = PADDLE_SPEED;
    } else {
        p->dx = 0;
    }

    p->x += p->dx * dt;
    if (p->x < 0) p->x = 0;
    if (p->x > gameScreenWidth - p->width) p->x = gameScreenWidth - p->width;
}

void InitBallQuads()
{
    int x = 96;
    int y = 48;
    int count = 0;
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 4; i++) {
            if (count < 7) {
                ballQuads[count++] = (Rectangle){ x + i * 8, y + j * 8, 8, 8 };
            }
        }
    }
}

void InitBall(Ball *b)
{
    b->x = gameScreenWidth / 2 - 4;
    b->y = gameScreenHeight / 2 - 4;
    b->dx = GetRandomValue(-200, 200);
    b->dy = GetRandomValue(-60, -50);
    b->width = 8;
    b->height = 8;
    b->skin = 0;
}

void UpdateBall(Ball *b, float dt)
{
    b->x += b->dx * dt;
    b->y += b->dy * dt;

    if (b->x <= 0) {
        b->x = 0;
        b->dx = -b->dx;
        PlaySound(wallHitSound);
    } else if (b->x + b->width >= gameScreenWidth) {
        b->x = gameScreenWidth - b->width;
        b->dx = -b->dx;
        PlaySound(wallHitSound);
    }

    if (b->y <= 0) {
        b->y = 0;
        b->dy = -b->dy;
        PlaySound(wallHitSound);
    }
}

void InitBrickQuads()
{
    int count = 0;
    for (int y = 0; y < 96 && count < BRICK_QUAD_COUNT; y += BRICK_HEIGHT) {
        for (int x = 0; x < 160 && count < BRICK_QUAD_COUNT; x += BRICK_WIDTH) {
            brickQuads[count++] = (Rectangle){ x, y, BRICK_WIDTH, BRICK_HEIGHT };
        }
    }
}


void InitBricks()
{
    brickCount = 0;
    int numRows = GetRandomValue(3, 5);
    int numCols = GetRandomValue(7, 13);
    if (numCols % 2 == 0) numCols++; // ensure columns odd

    // Level-dependent color/tier richness
    int highestTier = (level / 5 > 3) ? 3 : level / 5;
    int highestColor = ((level % 5) + 3 > 5) ? 5 : (level % 5) + 3;

    for (int y = 0; y < numRows; y++) {

        bool skipPattern = GetRandomValue(0, 1) == 1;
        bool alternatePattern = GetRandomValue(0, 1) == 1;

        int alternateColor1 = GetRandomValue(1, highestColor);
        int alternateColor2 = GetRandomValue(1, highestColor);
        int alternateTier1 = GetRandomValue(0, highestTier);
        int alternateTier2 = GetRandomValue(0, highestTier);

        bool skipFlag = GetRandomValue(0, 1) == 1;
        bool alternateFlag = GetRandomValue(0, 1) == 1;

        int solidColor = GetRandomValue(1, highestColor);
        int solidTier = GetRandomValue(0, highestTier);

        for (int x = 0; x < numCols; x++) {
            if (brickCount >= MAX_BRICKS) break;

            if (skipPattern && skipFlag) {
                skipFlag = !skipFlag;
                continue;
            } else {
                skipFlag = !skipFlag;
            }

            float bx = x * BRICK_WIDTH + 8 + (13 - numCols) * 16;
            float by = (y + 1) * BRICK_HEIGHT;

            int color, tier;
            if (alternatePattern) {
                if (alternateFlag) {
                    color = alternateColor1;
                    tier = alternateTier1;
                } else {
                    color = alternateColor2;
                    tier = alternateTier2;
                }
                alternateFlag = !alternateFlag;
            } else {
                color = solidColor;
                tier = solidTier;
            }

            int spriteIndex = (color - 1) * 4 + tier; // assuming 4 tiers per color

            bricks[brickCount++] = (Brick){
                .x = bx,
                .y = by,
                .width = BRICK_WIDTH,
                .height = BRICK_HEIGHT,
                .inPlay = true,
                .color = color,
                .tier = tier,
                .spriteIndex = spriteIndex
            };
        }
    }
}

void ServeState(float dt) {
    // Paddle movement
    UpdatePaddle(&playerPaddle, dt);

    // Place ball above paddle
    ball.x = playerPaddle.x + (playerPaddle.width / 2) - (ball.width / 2);
    ball.y = playerPaddle.y - ball.height;

    // Wait for enter to serve
    if (IsKeyPressed(KEY_ENTER)) {
        // Give ball a new velocity
        ball.dx = GetRandomValue(-200, 200);
        ball.dy = GetRandomValue(-60, -50);
        currentState = STATE_PLAY;
    }
}

void GameOverState()
{
    if (IsKeyPressed(KEY_ENTER)) {
        // Reset game state to initial values
        health = 3;
        score = 0;
        InitPaddle(&playerPaddle);
        InitBall(&ball);
        level++; InitBricks();
        currentState = STATE_START;
    }
}

/* DRAW FUNCTIONS */

void DrawFPSCustom()
{
    char fpsText[16];
    sprintf(fpsText, "%d FPS", GetFPS());
    DrawTextEx(smallFont, fpsText, (Vector2){5, 5}, 8, 1, GREEN);
}

void DrawStartMenu()
{
    // Title
    const char *title = "BREAKOUT";
    Vector2 titleSize = MeasureTextEx(largeFont, title, 32, 1);
    float titleX = (gameScreenWidth - titleSize.x) / 2;
    float titleY = gameScreenHeight / 3;
    DrawTextEx(largeFont, title, (Vector2){titleX, titleY}, 32, 1, WHITE);

    // Option 1: START
    const char *startText = "START";
    Vector2 startSize = MeasureTextEx(mediumFont, startText, 16, 1);
    float startX = (gameScreenWidth - startSize.x) / 2;
    float startY = gameScreenHeight / 2 + 70;

    Color startColor = (startMenu.highlighted == 1) ? (Color){103, 255, 255, 255} : WHITE;
    DrawTextEx(mediumFont, startText, (Vector2){startX, startY}, 16, 1, startColor);

    // Option 2: HIGH SCORES
    const char *scoreText = "HIGH SCORES";
    Vector2 scoreSize = MeasureTextEx(mediumFont, scoreText, 16, 1);
    float scoreX = (gameScreenWidth - scoreSize.x) / 2;
    float scoreY = gameScreenHeight / 2 + 90;

    Color scoreColor = (startMenu.highlighted == 2) ? (Color){103, 255, 255, 255} : WHITE;
    DrawTextEx(mediumFont, scoreText, (Vector2){scoreX, scoreY}, 16, 1, scoreColor);
}

void DrawGame()
{
    DrawPaddle(&playerPaddle);
    DrawBall(&ball);
    DrawBricks();

    DrawHealth();

    Vector2 scorePosition = {gameScreenWidth - 60, 5};
    DrawTextEx(smallFont, TextFormat("Score: %d", score), scorePosition, 8, 1, WHITE);

    if (isPaused)
    {
        const char *msg = "PAUSED";
        Vector2 size = MeasureTextEx(largeFont, msg, 32, 1);
        Vector2 position = { (gameScreenWidth - size.x)/2, gameScreenHeight/2 - 16 };
        DrawTextEx(largeFont, msg, position, 32, 1, blueColor);
    }
}

void DrawBall(Ball *b)
{
    DrawTextureRec(mainTexture, ballQuads[b->skin], (Vector2){ b->x, b->y }, WHITE);
}

void DrawPaddle(Paddle *p)
{
    int index = (p->size - 1) + 4 * (p->skin - 1);
    DrawTextureRec(mainTexture, paddleQuads[index], (Vector2){ p->x, p->y }, WHITE);
}

void DrawBricks()
{
    for (int i = 0; i < brickCount; i++) {
        if (bricks[i].inPlay) {
            DrawTextureRec(
            mainTexture,
            brickQuads[bricks[i].spriteIndex],
            (Vector2){ bricks[i].x, bricks[i].y },
            WHITE
        );
        }
    }
}

void DrawHealth()
{
    // The first frame is a full heart, the second is an empty heart.
    Rectangle fullHeart = { 0, 0, 10, 9 };
    Rectangle emptyHeart = { 10, 0, 10, 9 };
    float x = gameScreenWidth - 100;
    for (int i = 0; i < health; i++) {
        DrawTextureRec(heartsTexture, fullHeart, (Vector2){ x, 4 }, WHITE);
        x += 11;
    }
    for (int i = 0; i < 3 - health; i++) {
        DrawTextureRec(heartsTexture, emptyHeart, (Vector2){ x, 4 }, WHITE);
        x += 11;
    }
}

void DrawServe()
{
    DrawPaddle(&playerPaddle);
    DrawBall(&ball);
    DrawBricks();
    DrawHealth();

    // Draw score at top right
    Vector2 scorePosition = {gameScreenWidth - 60, 5};
    DrawTextEx(smallFont, TextFormat("Score: %d", score), scorePosition, 8, 1, WHITE);

    // Draw serve message
    const char* msg = "Press Enter to serve!";
    int textWidth = MeasureText(msg, 20);
    Vector2 position = {(gameScreenWidth - textWidth) / 2, gameScreenHeight / 2};
    DrawTextEx(mediumFont, msg, position, 16, 1, WHITE);
}

void DrawGameOver()
{
    int centerX = gameScreenWidth / 2;
    int y1 = gameScreenHeight / 3;
    int y2 = gameScreenHeight / 2;
    int y3 = gameScreenHeight - gameScreenHeight / 4;
    const char* msg1 = "GAME OVER";
    const char* msg2 = TextFormat("Final Score: %d", score);
    const char* msg3 = "Press Enter!";
    DrawTextEx(largeFont, msg1, (Vector2){centerX - MeasureText(msg1, 32)/2, y1}, 32, 1, WHITE);
    DrawTextEx(mediumFont, msg2,(Vector2){centerX - MeasureText(msg2, 20)/2, y2}, 16, 1, WHITE);
    DrawTextEx(mediumFont, msg3,(Vector2){centerX - MeasureText(msg3, 20)/2, y3}, 16, 1, WHITE);
}

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

#define PADDLE_SPEED 200.0f
#define PADDLE_SKINS 4
#define PADDLE_SIZES 4
Rectangle paddleQuads[PADDLE_SKINS * PADDLE_SIZES];

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

int main(void) {
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

    InitPaddleQuads();
    InitPaddle(&playerPaddle);

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
            case STATE_PLAY:
                GameLogic(deltaTime);
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

void GameLogic(float dt)
{
    UpdatePaddle(&playerPaddle, dt);
}

void UpdateStartMenu(void)
{
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        startMenu.highlighted = (startMenu.highlighted == 1) ? 2 : 1;
        PlaySound(paddleHitSound);
    }

    if (IsKeyPressed(KEY_ENTER)) {
        PlaySound(confirmSound);
        if (startMenu.highlighted == 1) {
            currentState = STATE_PLAY;
        }
    }
}

void InitPaddleQuads() {
    int counter = 0;
    for (int i = 0; i < PADDLE_SKINS; i++) {
        int y = 64 + i * 32;

        paddleQuads[counter++] = (Rectangle){ 0,      y, 32, 16 };  // small
        paddleQuads[counter++] = (Rectangle){ 32,     y, 64, 16 };  // medium
        paddleQuads[counter++] = (Rectangle){ 96,     y, 96, 16 };  // large
        paddleQuads[counter++] = (Rectangle){ 0,  y + 16,128, 16 }; // huge
    }
}

void InitPaddle(Paddle *p) {
    p->x = gameScreenWidth / 2 - 32;
    p->y = gameScreenHeight - 32;
    p->dx = 0;
    p->width = 64;
    p->height = 16;
    p->skin = 1;
    p->size = 2;
}

void UpdatePaddle(Paddle *p, float dt) {
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

void DrawPaddle(Paddle *p) {
    int index = (p->size - 1) + 4 * (p->skin - 1);
    DrawTextureRec(mainTexture, paddleQuads[index], (Vector2){ p->x, p->y }, WHITE);
}

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

    if (isPaused)
    {
        const char *msg = "PAUSED";
        Vector2 size = MeasureTextEx(largeFont, msg, 32, 1);
        Vector2 position = { (gameScreenWidth - size.x)/2, gameScreenHeight/2 - 16 };
        DrawTextEx(largeFont, msg, position, 32, 1, blueColor);
    }
}

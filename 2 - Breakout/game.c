#include "game.h"
#include "raylib.h"
#include <stdio.h>

const int screenWidth = 1280;
const int screenHeight = 720;

int gameScreenWidth = 432;
int gameScreenHeight = 243;

bool isPaused = false;
Sound pauseSound;
Texture2D pauseTexture;

GameState currentState = STATE_TITLE;

typedef struct {
    int highlighted;  // 1 for "START", 2 for "HIGH SCORES"
} StartState;

StartState state;

Color blueColor = {103, 255, 255, 255};

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
    pauseTexture = LoadTexture("res/graphics/pause.png");

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

    StartState state = { .highlighted = 1 };

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
    UnloadTexture(pauseTexture);

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
    if (IsKeyPressed(KEY_P)) {
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
            case STATE_TITLE:
                if (IsKeyPressed(KEY_ENTER)) {
                    currentState = STATE_PLAY;
                }
                break;
            case STATE_PLAY:
                GameLogic(deltaTime);
                break;
        }
    }

    BeginTextureMode(target);
        DrawFPSCustom();
        if (isPaused)
        {
            // DrawRectangle(0, 0, gameScreenWidth, gameScreenHeight, Fade(BLACK, 0.6f));

            float scale = 0.09f; // Adjust size to 50%
            int pauseTextureWidth = (int)(pauseTexture.width * scale);
            int pauseTextureHeight = (int)(pauseTexture.height * scale);
            Vector2 position = {
                (gameScreenWidth - pauseTextureWidth) / 2.0f,
                (gameScreenHeight - pauseTextureHeight) / 2.0f
            };

            DrawTextureEx(pauseTexture, position, 0.0f, scale, WHITE);
        }
        else if (currentState == STATE_TITLE)
            DrawTitle();
        else if (currentState == STATE_PLAY)
            DrawGame();
    EndTextureMode();

    BeginDrawing();
        ClearBackground(WHITE);     // Clear screen background

        // Draw render texture to screen, properly scaled
        DrawTexturePro(target.texture, (Rectangle){ 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height },
                       (Rectangle){ (GetScreenWidth() - ((float)gameScreenWidth*scale))*0.5f, (GetScreenHeight() - ((float)gameScreenHeight*scale))*0.5f,
                       (float)gameScreenWidth*scale, (float)gameScreenHeight*scale }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndDrawing();
}

void GameLogic(float dt)
{
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN))
    {
        // Toggle the highlighted option (if 1, switch to 2; otherwise, switch to 1)
        state.highlighted = (state.highlighted == 1) ? 2 : 1;
        PlaySound(paddleHitSound);
    }
}

void DrawFPSCustom()
{
    char fpsText[16];
    sprintf(fpsText, "%d FPS", GetFPS());
    DrawTextEx(smallFont, fpsText, (Vector2){5, 5}, 8, 1, GREEN);
}

void DrawTitle()
{
    ClearBackground(BLACK);
    Rectangle src = { 0.0f, 0.0f, (float)backgroundTexture.width, (float)backgroundTexture.height };
    Rectangle dst = { 0.0f, 0.0f, (float)gameScreenWidth + 1, (float)gameScreenHeight + 1 };
    DrawTexturePro(backgroundTexture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);

    static int highlighted = 1;  // 1 = "START", 2 = "HIGH SCORES"

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        highlighted = (highlighted == 1) ? 2 : 1;
        PlaySound(paddleHitSound);
    }

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
    float startY = gameScreenHeight / 2 + 20;

    Color startColor = (highlighted == 1) ? (Color){103, 255, 255, 255} : WHITE;
    DrawTextEx(mediumFont, startText, (Vector2){startX, startY}, 16, 1, startColor);

    // Option 2: HIGH SCORES
    const char *scoreText = "HIGH SCORES";
    Vector2 scoreSize = MeasureTextEx(mediumFont, scoreText, 16, 1);
    float scoreX = (gameScreenWidth - scoreSize.x) / 2;
    float scoreY = startY + 20;

    Color scoreColor = (highlighted == 2) ? (Color){103, 255, 255, 255} : WHITE;
    DrawTextEx(mediumFont, scoreText, (Vector2){scoreX, scoreY}, 16, 1, scoreColor);
}


void DrawGame()
{
    ClearBackground(BLACK);
}

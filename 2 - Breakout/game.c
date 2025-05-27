#include "game.h"
#include "raylib.h"
#include <stdio.h>

const int screenWidth = 1280;
const int screenHeight = 720;

int gameScreenWidth = 512;
int gameScreenHeight = 288;

bool isPaused = false;
Sound pauseSound;
Texture2D pauseTexture;

GameState currentState = STATE_TITLE;

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
    // logic
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
}

void DrawGame()
{
    ClearBackground(BLACK);
}

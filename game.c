#include "game.h"
#include "raylib.h"

#define MAX(a, b) ((a)>(b)? (a) : (b))
#define MIN(a, b) ((a)<(b)? (a) : (b))

const int screenWidth = 1280;
const int screenHeight = 720;

int gameScreenWidth = 512;
int gameScreenHeight = 288;

Texture2D background;
Texture2D ground;

int main(void) {
    SetTraceLogLevel(LOG_ALL);

    /* Initialization: Set up the window and load game resources. */

    InitWindow(screenWidth, screenHeight, "Flappy Bird");


    // Render texture initialization, used to hold the rendering result so we can easily resize it
    RenderTexture2D target = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);  // Texture scale filter to use

    background = LoadTexture("res/background.png");
    SetTextureFilter(background, TEXTURE_FILTER_POINT);
    ground = LoadTexture("res/ground.png");
    SetTextureFilter(ground, TEXTURE_FILTER_POINT);

    SetTargetFPS(60);

    while (!WindowShouldClose()) {

        UpdateDrawFrame(target);
    }

    /* De-Initialization: Clean up resources and close the window. */

    CloseWindow(); // Close window and OpenGL context

    return 0;
}

void UpdateDrawFrame(RenderTexture2D target)
{
    float deltaTime = GetFrameTime();
    // Compute required framebuffer scaling
    float scale = MIN((float)GetScreenWidth()/gameScreenWidth, (float)GetScreenHeight()/gameScreenHeight);

    
    GameLogic(deltaTime);

    BeginTextureMode(target);
        DrawGame();
    EndTextureMode();

    BeginDrawing();
        ClearBackground(BLACK);     // Clear screen background

        // Draw render texture to screen, properly scaled
        DrawTexturePro(target.texture, (Rectangle){ 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height },
                       (Rectangle){ (GetScreenWidth() - ((float)gameScreenWidth*scale))*0.5f, (GetScreenHeight() - ((float)gameScreenHeight*scale))*0.5f,
                       (float)gameScreenWidth*scale, (float)gameScreenHeight*scale }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndDrawing();
}

void GameLogic(float deltaTime)
{

}

void DrawGame()
{
    ClearBackground(SKYBLUE);
    DrawTexture(background, 0, 0, WHITE);
    DrawTexture(ground, 0, gameScreenHeight - 16, WHITE);
}
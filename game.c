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
float backgroundScroll = 0.0f;
float groundScroll = 0.0f;

#define BACKGROUND_SCROLL_SPEED 30
#define GROUND_SCROLL_SPEED 60
#define BACKGROUND_LOOPING_POINT 413

Bird bird;

#define GRAVITY 20

Pipe pipe;
Pipe pipes[10] = {0};
int pipesCount = 0;
Texture2D pipeTexture;
float spawnTimer = 0.0f;

int main(void) {
    SetTraceLogLevel(LOG_ALL);

    /* Initialization: Set up the window and load game resources. */
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Flappy Bird");


    // Render texture initialization, used to hold the rendering result so we can easily resize it
    RenderTexture2D target = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);  // Texture scale filter to use

    background = LoadTexture("res/background.png");
    ground = LoadTexture("res/ground.png");
    pipeTexture = LoadTexture("res/pipe.png");

    InitBird(&bird);

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

void GameLogic(float dt)
{
    // scroll background by preset speed * dt, looping back to 0 after the looping point
    backgroundScroll = fmodf((backgroundScroll + BACKGROUND_SCROLL_SPEED * dt), BACKGROUND_LOOPING_POINT);
    // printf("backgroundScroll: %f\n", backgroundScroll);

    // scroll ground by preset speed * dt, looping back to 0 after the screen width passes
    groundScroll = fmodf((groundScroll + GROUND_SCROLL_SPEED * dt), gameScreenWidth);
    // printf("groundScroll: %f\n", groundScroll);

    spawnTimer = spawnTimer + dt;
    if (spawnTimer > 2)
    {
        if (pipesCount < 10) {
            pipes[pipesCount++] = InitPipe();
            printf("Added new pipe!\n");
        }
        spawnTimer = 0;
    }

    UpdateBird(dt, &bird);

    for (int i = 0; i < pipesCount; ++i)
    {
        UpdatePipe(dt, &pipes[i]);
        if (pipes[i].x < -pipes[i].width)
        {
            for (int j = i; i < pipesCount - 1; i++) {
                pipes[i] = pipes[i + 1];
            }
            pipesCount--;
        }
    }
    printf("After removal:\n");
    for (int i = 0; i < pipesCount; i++) {
        printf("Pipe id: %d\n", i);
    }
}

void DrawGame()
{
    ClearBackground(SKYBLUE);
    DrawTexture(background, -(int)backgroundScroll, 0, WHITE);
    DrawTexture(ground, -(int)groundScroll, gameScreenHeight - 16, WHITE);
    DrawText(TextFormat("Background Scroll: %2.1f", backgroundScroll), 10, 10, 10, BLACK);
    DrawText(TextFormat("Ground Scroll: %2.1f", groundScroll), 10, 30, 10, BLACK);

    DrawBird(&bird);

    // render all the pipes in scene
    for (int i = 0; i < pipesCount; ++i)
    {
        DrawPipe(&pipes[i]);
    }
    
}

void InitBird(Bird *bird)
{
    bird->image = LoadTexture("res/bird.png");
    bird->width = 38;
    bird->height = 24;
    bird->x = gameScreenWidth / 2 - (38 / 2);
    bird->y = gameScreenHeight / 2 - (24 / 2);
    bird->dy = 0;
}

void UpdateBird(float dt, Bird *bird)
{
    // apply gravity to velocity
    bird->dy += GRAVITY * dt;
    // add a sudden burst of negative gravity if we hit space
    if (IsKeyPressed(KEY_SPACE))
    {
        bird->dy = -5;
    }
    // apply current velocity to Y position
    bird->y += bird->dy;
}

void DrawBird(Bird *bird)
{
    DrawTexture(bird->image, bird->x, bird->y, WHITE);
}

Pipe InitPipe()
{
    Pipe newPipe;
    newPipe.image = pipeTexture;
    newPipe.scroll = -60;
    newPipe.x = gameScreenWidth;
    newPipe.y = GetRandomValue(gameScreenHeight / 4, gameScreenHeight - 10);
    newPipe.width = 70;
    return newPipe;
}

void UpdatePipe(float dt, Pipe *pipe)
{
    pipe->x += pipe->scroll * dt;
}

void DrawPipe(Pipe *pipe)
{
    DrawTexture(pipe->image, pipe->x, pipe->y, WHITE);
}

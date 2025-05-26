#include "game.h"
#include "raylib.h"

#define MAX(a, b) ((a)>(b)? (a) : (b))
#define MIN(a, b) ((a)<(b)? (a) : (b))

const int screenWidth = 1280;
const int screenHeight = 720;

int gameScreenWidth = 512;
int gameScreenHeight = 288;

bool isPaused = false;
Sound pauseSound;
Texture2D pauseIcon;

Texture2D background;
Texture2D ground;
float backgroundScroll = 0.0f;
float groundScroll = 0.0f;

// speed at which we should scroll our images, scaled by dt
#define BACKGROUND_SCROLL_SPEED 30
#define GROUND_SCROLL_SPEED 60
// point at which we should loop our background back to X 0
#define BACKGROUND_LOOPING_POINT 413
// point at which we should loop our ground back to X 0
#define GROUND_LOOPING_POINT 514

Bird bird;

#define GRAVITY 20

#define PIPE_SPEED 60
#define PIPE_HEIGHT 288
#define PIPE_WIDTH 70

Pipe pipe;
Pipe pipes[10][2] = {0};
int pipesCount = 0;
Texture2D pipeTexture;
float spawnTimer = 0.0f;
float pipeSpawnInterval = 2.0f;
int pipeGapHeight;

int lastY;

int score;

GameState currentState = STATE_TITLE;
Font smallFont;
Font mediumFont;
Font flappyFont;

float COUNTDOWN_TIME = 0.75f;
int count;
float timer;

Sound jumpSound;
Sound scoreSound;
Sound explosionSound;
Sound hurtSound;
Music music;

Texture2D medalBronze;
Texture2D medalSilver;
Texture2D medalGold;

int main(void) {
    SetTraceLogLevel(LOG_ALL);

    /* Initialization: Set up the window and load game resources. */
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Flappy Bird");

    InitAudioDevice();

    // Pause
    pauseSound = LoadSound("res/pause.mp3");
    pauseIcon = LoadTexture("res/pause.png");

    // Retro Fonts
    smallFont = LoadFont("res/font.ttf");
    mediumFont = LoadFontEx("res/flappy.ttf", 14, 0, 0);
    flappyFont = LoadFontEx("res/flappy.ttf", 28, 0, 0);

    // Sounds / Music
    jumpSound = LoadSound("res/jump.wav");
    scoreSound = LoadSound("res/score.wav");
    explosionSound = LoadSound("res/explosion.wav");
    hurtSound = LoadSound("res/hurt.wav");
    music = LoadMusicStream("res/marios_way.mp3");

    // Start music
    music.looping = true;
    PlayMusicStream(music);

    // Medals
    medalBronze = LoadTexture("res/flat_medal3.png");
    medalSilver = LoadTexture("res/flat_medal2.png");
    medalGold   = LoadTexture("res/flat_medal1.png");

    // Render texture initialization, used to hold the rendering result so we can easily resize it
    RenderTexture2D target = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);  // Texture scale filter to use

    background = LoadTexture("res/background.png");
    ground = LoadTexture("res/ground.png");
    pipeTexture = LoadTexture("res/pipe.png");
    srand(time(NULL));
    lastY = -PIPE_HEIGHT + rand() % 80 + 20;
    score = 0;
    count = 0;
    timer = 0.0f;

    InitBird(&bird);

    SetTargetFPS(60);

    while (!WindowShouldClose()) {

        UpdateDrawFrame(target);
    }

    /* De-Initialization: Clean up resources and close the window. */
    UnloadTexture(background);
    UnloadTexture(ground);
    UnloadTexture(pipeTexture);
    UnloadTexture(bird.image);
    UnloadFont(smallFont);
    UnloadFont(mediumFont);
    UnloadFont(flappyFont);
    UnloadSound(jumpSound);
    UnloadSound(scoreSound);
    UnloadSound(explosionSound);
    UnloadSound(hurtSound);
    UnloadMusicStream(music);
    UnloadTexture(medalBronze);
    UnloadTexture(medalSilver);
    UnloadTexture(medalGold);

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
                ScrollingBackground(deltaTime);
                if (IsKeyPressed(KEY_ENTER)) {
                    currentState = STATE_COUNTDOWN;
                    ResetGame();
                }
                break;
            case STATE_PLAY:
                GameLogic(deltaTime);
                break;
            case STATE_SCORE:
                ScrollingBackground(deltaTime);
                if (IsKeyPressed(KEY_ENTER)) {
                    currentState = STATE_COUNTDOWN;
                    ResetGame();
                }
                break;
            case STATE_COUNTDOWN:
                ScrollingBackground(deltaTime);
                timer += deltaTime;
                if (timer > COUNTDOWN_TIME) {
                    timer = fmod(timer, COUNTDOWN_TIME);
                    count--;

                    if (count == 0)
                        currentState = STATE_PLAY;
                }
        }
    }

    BeginTextureMode(target);
        if (isPaused)
        {
            float scale = 0.1f; // Adjust size to 50%
            int iconWidth = (int)(pauseIcon.width * scale);
            int iconHeight = (int)(pauseIcon.height * scale);
            Vector2 position = {
                (gameScreenWidth - iconWidth) / 2.0f,
                (gameScreenHeight - iconHeight) / 2.0f
            };
            DrawTextureEx(pauseIcon, position, 0.0f, scale, WHITE);
        }
        else if (currentState == STATE_TITLE)
            DrawTitle();
        else if (currentState == STATE_PLAY)
            DrawGame();
        else if (currentState == STATE_SCORE)
            DrawScore();
        else if (currentState == STATE_COUNTDOWN)
            DrawCountdown();
    EndTextureMode();

    BeginDrawing();
        ClearBackground(BLACK);     // Clear screen background

        // Draw render texture to screen, properly scaled
        DrawTexturePro(target.texture, (Rectangle){ 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height },
                       (Rectangle){ (GetScreenWidth() - ((float)gameScreenWidth*scale))*0.5f, (GetScreenHeight() - ((float)gameScreenHeight*scale))*0.5f,
                       (float)gameScreenWidth*scale, (float)gameScreenHeight*scale }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndDrawing();
}

void ScrollingBackground(float dt)
{
    // scroll background by preset speed * dt, looping back to 0 after the looping point
    backgroundScroll = fmodf((backgroundScroll + BACKGROUND_SCROLL_SPEED * dt), BACKGROUND_LOOPING_POINT);

    // scroll ground by preset speed * dt, looping back to 0 after the screen width passes
    groundScroll = fmodf((groundScroll + GROUND_SCROLL_SPEED * dt), GROUND_LOOPING_POINT);
}
void GameLogic(float dt)
{
    ScrollingBackground(dt);

    spawnTimer = spawnTimer + dt;
    if (spawnTimer > pipeSpawnInterval)
    {
        if (pipesCount < 10) {
            pipeGapHeight = GetRandomValue(80, 120);
            // Ensure the top pipe is placed correctly
            int topPipeY = MAX(-PIPE_HEIGHT + 10, MIN(lastY + GetRandomValue(-20, 20), gameScreenHeight - pipeGapHeight - PIPE_HEIGHT));
            lastY = topPipeY;
            // Create top pipe at topPipeY
            pipes[pipesCount][0] = InitPipe(topPipeY, 1);
            // Create bottom pipe at the position below top pipe, plus gap
            pipes[pipesCount][1] = InitPipe(topPipeY + PIPE_HEIGHT + pipeGapHeight, 0);
            pipesCount++;
        }
        spawnTimer = 0;
        pipeSpawnInterval = GetRandomValue(15, 25) / 10.0f;
    }

    UpdateBird(dt, &bird);

    // collision between bird and pipes
    for (int i = 0; i < pipesCount; ++i)
    {
        UpdatePipe(dt, &pipes[i][0]);
        UpdatePipe(dt, &pipes[i][1]);

        if (CollideBird(&bird, &pipes[i][0]) || CollideBird(&bird, &pipes[i][1]))
        {
            currentState = STATE_SCORE;
            PlaySound(explosionSound);
            PlaySound(hurtSound);
            // ResetGame();
        }

        if (not pipes[i][0].scored and ((pipes[i][0].x + PIPE_WIDTH) < bird.x)) {
            score++;
            pipes[i][0].scored = true;
            PlaySound(scoreSound);
        }

        if (pipes[i][0].x < -pipes[i][0].width)
        {
            for (int j = i; j < pipesCount - 1; j++) {
                pipes[j][0] = pipes[j + 1][0];
                pipes[j][1] = pipes[j + 1][1];
            }
            pipesCount--;
            i--;
        }
    }

    // reset if we get to the ground
    if (bird.y > gameScreenHeight - ground.height)
    {
        currentState = STATE_SCORE;
        PlaySound(explosionSound);
        PlaySound(hurtSound);
        // ResetGame();
    }
}

void DrawTitle()
{
    ClearBackground(SKYBLUE);
    DrawTexture(background, -(int)backgroundScroll, 0, WHITE);
    DrawTexture(ground, -(int)groundScroll, gameScreenHeight - 16, WHITE);

    Vector2 titleSize = MeasureTextEx(flappyFont, "Flappy Bird", 28, 0);
    Vector2 titlePos = {
        (gameScreenWidth - titleSize.x) / 2,
        64
    };
    DrawTextEx(flappyFont, "Flappy Bird", titlePos, 28, 0, WHITE);

    Vector2 promptSize = MeasureTextEx(mediumFont, "Press Enter", 14, 0);
    Vector2 promptPos = {
        (gameScreenWidth - promptSize.x) / 2,
        100
    };
    DrawTextEx(mediumFont, "Press Enter", promptPos, 14, 0, WHITE);
}

void DrawScore()
{
    ClearBackground(SKYBLUE);
    DrawTexture(background, -(int)backgroundScroll, 0, WHITE);
    DrawTexture(ground, -(int)groundScroll, gameScreenHeight - 16, WHITE);

    Vector2 scoreSize = MeasureTextEx(flappyFont, "Oof! You lost!", 28, 0);
    Vector2 scorePos = {
        (gameScreenWidth - scoreSize.x) / 2,
        64
    };
    DrawTextEx(flappyFont, "Oof! You lost!", scorePos, 28, 0, WHITE);

    Vector2 score1Size = MeasureTextEx(mediumFont, "Score:  ", 14, 0);
    Vector2 score1Pos = {
        (gameScreenWidth - score1Size.x) / 2,
        100
    };
    DrawTextEx(mediumFont, TextFormat("Score: %d", score), score1Pos, 14, 0, WHITE);

    Vector2 promptSize = MeasureTextEx(mediumFont, "Press Enter to Play Again!", 14, 0);
    Vector2 promptPos = {
        (gameScreenWidth - promptSize.x) / 2,
        160
    };
    DrawTextEx(mediumFont, "Press Enter to Play Again!", promptPos, 14, 0, WHITE);

    Texture2D medalToDraw;
    bool showMedal = false;

    if (score >= 9) {
        medalToDraw = medalGold;
        showMedal = true;
    } else if (score >= 6) {
        medalToDraw = medalSilver;
        showMedal = true;
    } else if (score >= 3) {
        medalToDraw = medalBronze;
        showMedal = true;
    }

    if (showMedal) {
        int medalX = (gameScreenWidth - medalToDraw.width) / 2;
        int medalY = 180;  // below the score text
        DrawTexture(medalToDraw, medalX, medalY, WHITE);
    }

}

void DrawCountdown()
{
    ClearBackground(SKYBLUE);
    DrawTexture(background, -(int)backgroundScroll, 0, WHITE);
    DrawTexture(ground, -(int)groundScroll, gameScreenHeight - 16, WHITE);

    char buffer[8];
    sprintf(buffer, "%d", count);
    Vector2 countSize = MeasureTextEx(flappyFont, buffer, 28, 0);
    Vector2 countPos = {
        (gameScreenWidth - countSize.x) / 2,
        120
    };
    DrawTextEx(flappyFont, buffer, countPos, 28, 0, WHITE);
}

void DrawGame()
{
    ClearBackground(SKYBLUE);
    DrawTexture(background, -(int)backgroundScroll, 0, WHITE);

    // render all the pipes in scene
    for (int i = 0; i < pipesCount; ++i)
    {
        DrawPipe(&pipes[i][0]);
        DrawPipe(&pipes[i][1]);
    }

    DrawTexture(ground, -(int)groundScroll, gameScreenHeight - 16, WHITE);

    DrawTextEx(flappyFont, TextFormat("Score: %d", score), (Vector2){10, 10}, 28, 0, WHITE);

    DrawBird(&bird);
    
}

void ResetGame(void)
{
    // backgroundScroll = 0.0f;
    // groundScroll = 0.0f;
    spawnTimer = 0.0f;
    pipesCount = 0;
    score = 0;
    lastY = -PIPE_HEIGHT + rand() % 80 + 20;
    UnloadTexture(bird.image); // could be optimized
    InitBird(&bird);
    count = 3;
    timer = 0.0f;
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
    if (IsKeyPressed(KEY_SPACE) or IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        bird->dy = -5;
        PlaySound(jumpSound);
    }
    // apply current velocity to Y position
    bird->y += bird->dy;
}

bool CollideBird(Bird *bird, Pipe *pipe)
{
    if (((bird->x + 2) + (bird->width - 4)) >= pipe->x &&
        ((bird->x + 2) <= (pipe->x + PIPE_WIDTH)) &&
        ((bird->y + 2) + (bird->height - 4)) >= pipe->y &&
        ((bird->y + 2) <= (pipe->y + PIPE_HEIGHT)))
    {
        return true;
    }
    return false;
}

void DrawBird(Bird *bird)
{
    DrawTexture(bird->image, bird->x, bird->y, WHITE);
}

Pipe InitPipe(int y, int flipped)
{
    Pipe newPipe;
    newPipe.image = pipeTexture;
    newPipe.scroll = -PIPE_SPEED;
    newPipe.x = gameScreenWidth + 32;
    newPipe.y = y;
    newPipe.width = PIPE_WIDTH;
    newPipe.height = PIPE_HEIGHT;
    newPipe.flipped = flipped;
    return newPipe;
}

void UpdatePipe(float dt, Pipe *pipe)
{
    pipe->x += pipe->scroll * dt;
}

void DrawPipe(Pipe *pipe)
{
    float height = pipe->flipped ? -pipe->height : pipe->height;
    DrawTextureRec(pipe->image, (Rectangle){0, 0, pipe->width, height}, (Vector2){pipe->x, pipe->y}, WHITE);
}

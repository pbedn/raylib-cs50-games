#include "game.h"
#include "raylib.h"
#include <math.h>
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

ParticleSystem ps;
Texture2D particleTexture;

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

HighScore highScores[HIGH_SCORE_COUNT];
int newHighScoreIndex = -1; // Position where the new score will go
int newScore = 0;
char nameChars[3] = { 'A', 'A', 'A' }; // Initial letters
int highlightedChar = 0; // 0..2, which char is being edited

int main() {
    SetTraceLogLevel(LOG_DEBUG);

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
    // PlayMusicStream(music);

    // Render texture initialization, used to hold the rendering result so we can easily resize it
    RenderTexture2D target = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);  // Texture scale filter to use

    InitGameState();
    LoadHighScores();
    InitPaddleQuads();
    InitPaddle(&playerPaddle);
    InitBallQuads();
    InitBall(&ball);
    InitBrickQuads();
    InitBricks();
    InitParticleSystem(&ps, particleTexture, (Vector2){100, 100});

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
            case STATE_VICTORY:
                VictoryState();
                break;
            case STATE_HIGH_SCORES:
                UpdateHighScores();
                break;
            case STATE_ENTER_HIGH_SCORE:
                UpdateEnterHighScore();
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
        else if (currentState == STATE_PLAY) {
            DrawGame();
            DrawParticleSystem(&ps);
        }
        else if (currentState == STATE_SERVE)
            DrawServe();
        else if (currentState == STATE_GAME_OVER)
            DrawGameOver();
        else if (currentState == STATE_VICTORY)
            DrawVictory();
        else if (currentState == STATE_HIGH_SCORES)
            DrawHighScores();
        else if (currentState == STATE_ENTER_HIGH_SCORE)
            DrawEnterHighScore();

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

    // Ball-Paddle Collision
    Rectangle ballRect = { ball.x, ball.y, ball.width, ball.height };
    Rectangle paddleRect = { playerPaddle.x, playerPaddle.y, (float)playerPaddle.width, (float)playerPaddle.height };
    if (CheckCollisionRecs(ballRect, paddleRect))
        HandleBallPaddleCollision(&ball, &playerPaddle);

    // Ball-Brick Collision with Edge Detection
    for (int i = 0; i < brickCount; i++) {
        if (bricks[i].inPlay) {
            Rectangle ballRect = { ball.x, ball.y, ball.width, ball.height };
            Rectangle brickRect = { bricks[i].x, bricks[i].y, bricks[i].width, bricks[i].height };
            if (CheckCollisionRecs(ballRect, brickRect)) {
                HandleBallBrickCollision(&ball, &bricks[i]);

                // Particle
                ps.emitterPos = (Vector2){bricks[i].x + 16, bricks[i].y + 8};
                Color startColor = (Color){0, 0, 255, 128}; // np. niebieski z alfa 128
                Color endColor = (Color){0, 0, 255, 0};     // transparentny niebieski
                    
                EmitParticle(&ps, startColor, endColor, 64);

                break; // tylko jedna kolizja w jednej klatce
            }
        }
    }

    UpdateParticleSystem(&ps, dt);

    if (CheckVictory() or IsKeyPressed(KEY_V))
    {
        PlaySound(victorySound);
        currentState = STATE_VICTORY;
        return;
    }

    // Dev Mode for debugging
    if (IsKeyPressed(KEY_B))
    {
        score = GetRandomValue(50, 500);
        currentState = STATE_GAME_OVER;
        return;
    }
}

void HandleBallPaddleCollision(Ball *ball, Paddle *playerPaddle)
{
    ball->y = playerPaddle->y - ball->height;
    ball->dy = -ball->dy;

    float paddleCenter = playerPaddle->x + playerPaddle->width / 2.0f;
    float ballCenter = ball->x + ball->width / 2;
    float diff = paddleCenter - ballCenter;

    if (ballCenter < paddleCenter && playerPaddle->dx < 0) {
        ball->dx = -50.0f - 8.0f * fabsf(diff);
    } else if (ballCenter > paddleCenter && playerPaddle->dx > 0) {
        ball->dx = 50.0f + 8.0f * fabsf(diff);
    }

    PlaySound(paddleHitSound);
}

void HandleBallBrickCollision(Ball *ball, Brick *brick)
{    
    // SCORING
    score += (brick->tier * 200 + brick->color * 25);
    // if we're at a higher tier than the base, we need to go down a tier
    // if we're already at the lowest color, else just go down a color
    if (brick->tier > 0) {
        if (brick->color == 1) {
            brick->tier--;
            brick->color = 5;
        } else {
            brick->color--;
        }
    }
    else {
        // if we're in the first tier and the base color, remove brick from play
        if (brick->color == 1)
            brick->inPlay = false;
        else
            brick->color--;
    }

    // TraceLog(LOG_DEBUG, "Brick hit: index=%d, color=%d, tier=%d, inPlay=%s",
    //          i, brick->color, brick->tier, brick->inPlay ? "true" : "false");


    // play a second layer sound if the brick is destroyed
    if (!brick->inPlay) {
        PlaySound(brickHit1Sound);
    } else {
        PlaySound(brickHit2Sound);
    }


    if (ball->x + ball->width - 1 < brick->x && ball->dx > 0) {
        // Hit left side
        ball->dx = -ball->dx;
        ball->x = brick->x - ball->width;
    } else if (ball->x + 1 > brick->x + brick->width && ball->dx < 0) {
        // Hit right side
        ball->dx = -ball->dx;
        ball->x = brick->x + brick->width;
    } else if (ball->y < brick->y) {
        // Hit top of brick, ball going down
        ball->dy = -ball->dy;
        ball->y = brick->y - ball->height;
    } else {
        // Hit bottom of brick, ball going up
        ball->dy = -ball->dy;
        ball->y = brick->y + brick->height;
    }

    // slight speed up
    ball->dy *= 1.02f;

    brick->spriteIndex = brick->tier * 5 + (brick->color - 1);
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
        } else { // highlighted == 2
            currentState = STATE_HIGH_SCORES;
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
    int highestTier = MIN(3, (int)floor(level / 5.0f));
    int highestColor = MIN(3, level % 5 + 3);

    for (int y = 0; y < numRows; y++) {

        bool skipPattern = GetRandomValue(0, 1) == 1;
        bool alternatePattern = GetRandomValue(0, 1) == 1;

        int alternateColor1 = GetRandomValue(1, highestColor);
        int alternateColor2 = GetRandomValue(1, highestColor);
        int alternateTier1 = GetRandomValue(1, highestTier);
        int alternateTier2 = GetRandomValue(1, highestTier);

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

            int spriteIndex = tier * 5 + (color - 1); // 5 colors (columns) × 4 tiers (rows) = 20 sprites

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
            TraceLog(LOG_DEBUG, "Brick %d → color=%d, tier=%d, spriteIndex=%d",
                     brickCount, color, tier, spriteIndex);
        }
    }
}

void InitParticleSystem(ParticleSystem* ps, Texture2D texture, Vector2 emitterPos) {
    ps->texture = texture;
    ps->emitterPos = emitterPos;
    ps->aliveCount = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        ps->particles[i].life = 0.0f;  // not active particles
    }
}

void EmitParticle(ParticleSystem* ps, Color colorStart, Color colorEnd, int count) {
    for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
        if (ps->particles[i].life <= 0) {
            ps->particles[i].position = ps->emitterPos;

            // Waterfall effect
            // ps->particles[i].velocity.x = GetRandomValue(-15, 15);
            // ps->particles[i].velocity.y = GetRandomValue(0, 40);

            // Round explosion
            float angle = GetRandomValue(0, 360) * DEG2RAD;
            float offsetRadius = GetRandomValue(0, 5); // up to 5px from center
            float speed = GetRandomValue(30, 100) / 10.0f; // 3.0 – 10.0 px/s
            ps->particles[i].velocity.x = cosf(angle) * speed * offsetRadius;
            ps->particles[i].velocity.y = sinf(angle) * speed * offsetRadius;


            ps->particles[i].life = ((float)GetRandomValue(5, 10)) / 10.0f; // 0.5 - 1 s
            ps->particles[i].colorStart = colorStart;
            ps->particles[i].colorEnd = colorEnd;
            ps->particles[i].sizeStart = 1.0f;
            ps->particles[i].sizeEnd = 0.0f;

            ps->aliveCount++;
            count--;
        }
    }
}

void UpdateParticleSystem(ParticleSystem* ps, float deltaTime) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &ps->particles[i];
        if (p->life > 0) {
            p->life -= deltaTime;
            if (p->life > 0) {
                p->position.x += p->velocity.x * deltaTime;
                p->position.y += p->velocity.y * deltaTime;

                // gravity acceleration
                p->velocity.y += 100 * deltaTime;  
            } else {
                ps->aliveCount--;
            }
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
        // Determine if score is in top 10
        newHighScoreIndex = -1;
        for (int i = 0; i < HIGH_SCORE_COUNT; i++) {
            if (score > highScores[i].score) {
                newHighScoreIndex = i;
                break;
            }
        }

        if (newHighScoreIndex != -1) {
            // Store score for entry state
            newScore = score;
            nameChars[0] = 'A';
            nameChars[1] = 'A';
            nameChars[2] = 'A';
            highlightedChar = 0;
            currentState = STATE_ENTER_HIGH_SCORE;
        } else {
            // No high score, return to start
            health = 3;
            score = 0;
            InitPaddle(&playerPaddle);
            InitBall(&ball);
            level = 1;
            InitBricks();
            currentState = STATE_START;
        }
    }
}

bool CheckVictory() {
    for (int i = 0; i < brickCount; ++i)
    {
        if (bricks[i].inPlay)
        {
            return false;
        }
    }
    return true;
}

void VictoryState() {
    UpdatePaddle(&playerPaddle, GetFrameTime());

    // Ball tracks paddle
    ball.x = playerPaddle.x + (playerPaddle.width / 2) - (ball.width / 2);
    ball.y = playerPaddle.y - ball.height;

    if (IsKeyPressed(KEY_ENTER)) {
        level++;
        InitBricks();  // create new level-dependent map
        currentState = STATE_SERVE;
    }
}

void UpdateHighScores(void)
{
    // Press Escape to return to the Start menu
    if (IsKeyPressed(KEY_ESCAPE)) {
        PlaySound(wallHitSound);
        currentState = STATE_START;
    }
}

void UpdateEnterHighScore(void)
{
    // Move between characters
    if (IsKeyPressed(KEY_LEFT)) {
        highlightedChar = (highlightedChar - 1 + 3) % 3;
        PlaySound(paddleHitSound);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        highlightedChar = (highlightedChar + 1) % 3;
        PlaySound(paddleHitSound);
    }

    // Change character letter
    if (IsKeyPressed(KEY_UP)) {
        nameChars[highlightedChar]++;
        if (nameChars[highlightedChar] > 'Z') nameChars[highlightedChar] = 'A';
        PlaySound(paddleHitSound);
    }
    if (IsKeyPressed(KEY_DOWN)) {
        nameChars[highlightedChar]--;
        if (nameChars[highlightedChar] < 'A') nameChars[highlightedChar] = 'Z';
        PlaySound(paddleHitSound);
    }

    // Confirm name entry
    if (IsKeyPressed(KEY_ENTER)) {
        // Shift lower scores down to make room
        for (int i = HIGH_SCORE_COUNT - 1; i > newHighScoreIndex; i--) {
            highScores[i] = highScores[i - 1];
        }

        // Insert new score and name
        highScores[newHighScoreIndex].name[0] = nameChars[0];
        highScores[newHighScoreIndex].name[1] = nameChars[1];
        highScores[newHighScoreIndex].name[2] = nameChars[2];
        highScores[newHighScoreIndex].name[3] = '\0';
        highScores[newHighScoreIndex].score = newScore;

        SaveHighScores();

        // Go to high scores view
        currentState = STATE_HIGH_SCORES;
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

void DrawParticleSystem(ParticleSystem* ps) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &ps->particles[i];
        if (p->life > 0) {
            float lifeRatio = p->life / 1.0f; // max life 1s

            Color c = {
                (unsigned char)(p->colorEnd.r + (p->colorStart.r - p->colorEnd.r) * lifeRatio),
                (unsigned char)(p->colorEnd.g + (p->colorStart.g - p->colorEnd.g) * lifeRatio),
                (unsigned char)(p->colorEnd.b + (p->colorStart.b - p->colorEnd.b) * lifeRatio),
                (unsigned char)(p->colorEnd.a + (p->colorStart.a - p->colorEnd.a) * lifeRatio)
            };

            float size = p->sizeEnd + (p->sizeStart - p->sizeEnd) * lifeRatio;

            DrawTextureEx(ps->texture, p->position, 0.0f, size, c);
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

void DrawVictory() {
    DrawPaddle(&playerPaddle);
    DrawBall(&ball);
    DrawBricks();
    DrawHealth();

    Vector2 scorePosition = {gameScreenWidth - 60, 5};
    DrawTextEx(smallFont, TextFormat("Score: %d", score), scorePosition, 8, 1, WHITE);

    // Message 1: "Level X Complete!"
    const char* msg1 = TextFormat("Level %d Complete!", level);
    Vector2 msg1Size = MeasureTextEx(largeFont, msg1, 32, 1);
    Vector2 msg1Pos = {
        (gameScreenWidth - msg1Size.x) / 2,
        gameScreenHeight / 4
    };
    DrawTextEx(largeFont, msg1, msg1Pos, 32, 1, WHITE);

    // Message 2: "Press Enter to serve!"
    const char* msg2 = "Press Enter to serve!";
    Vector2 msg2Size = MeasureTextEx(mediumFont, msg2, 16, 1);
    Vector2 msg2Pos = {
        (gameScreenWidth - msg2Size.x) / 2,
        gameScreenHeight / 2
    };
    DrawTextEx(mediumFont, msg2, msg2Pos, 16, 1, WHITE);
}

void DrawHighScores(void)
{
    // Title
    const char* title = "High Scores";
    Vector2 titleSize = MeasureTextEx(largeFont, title, 32, 1);
    DrawTextEx(largeFont, title,
               (Vector2){ (gameScreenWidth - titleSize.x)/2, 20 },
               32, 1, WHITE);

    // Layout constants (original style)
    float baseY = 60.0f;
    float lineH = 13.0f;
    float fixedExtraGap = 8.0f; // space after index, regardless of its width

    for (int i = 0; i < HIGH_SCORE_COUNT; ++i) {
        float y = baseY + (i+1)*lineH;

        // Index (1-based)
        const char* idx = TextFormat("%d.", i + 1);
        Vector2 idxSize = MeasureTextEx(mediumFont, idx, 16, 1);
        float idxX = gameScreenWidth * 0.25f;
        DrawTextEx(mediumFont, idx, (Vector2){ idxX, y }, 16, 1, WHITE);

        // Name (starts after index width + fixedExtraGap)
        const char* nm = (highScores[i].name[0]) ? highScores[i].name : "---";
        float nameX = idxX + idxSize.x + fixedExtraGap;
        DrawTextEx(mediumFont, nm, (Vector2){ nameX, y }, 16, 1, WHITE);

        // Score (right-aligned to 100px column starting at 0.5*width)
        const char* sc = (highScores[i].score > 0) ? TextFormat("%d", highScores[i].score) : "---";
        Vector2 scSize = MeasureTextEx(mediumFont, sc, 16, 1);
        float scRight = gameScreenWidth * 0.5f + 100.0f;
        DrawTextEx(mediumFont, sc,
            (Vector2){ scRight - scSize.x, y },
            16, 1, WHITE);
    }

    // Footer
    const char* hint = "Press Escape to return to the main menu!";
    Vector2 hintSize = MeasureTextEx(smallFont, hint, 8, 1);
    DrawTextEx(smallFont, hint,
               (Vector2){ (gameScreenWidth - hintSize.x)/2, gameScreenHeight - 18 },
               8, 1, WHITE);
}

void DrawEnterHighScore(void)
{
    const char* title = "New High Score!";
    Vector2 titleSize = MeasureTextEx(largeFont, title, 32, 1);
    DrawTextEx(largeFont, title,
               (Vector2){ (gameScreenWidth - titleSize.x) / 2, 20 },
               32, 1, WHITE);

    const char* scoreText = TextFormat("Your Score: %d", newScore);
    Vector2 scoreSize = MeasureTextEx(mediumFont, scoreText, 16, 1);
    DrawTextEx(mediumFont, scoreText,
               (Vector2){ (gameScreenWidth - scoreSize.x) / 2, 70 },
               16, 1, WHITE);

    // Draw name letters
    float startX = gameScreenWidth / 2 - 30;
    for (int i = 0; i < 3; i++) {
        Color color = (i == highlightedChar) ? YELLOW : WHITE;
        char letterStr[2] = { nameChars[i], '\0' };
        Vector2 letterSize = MeasureTextEx(largeFont, letterStr, 32, 1);
        DrawTextEx(largeFont, letterStr,
                   (Vector2){ startX + i * 30, 120 },
                   32, 1, color);
    }

    const char* hint = "Use arrow keys to set name, Enter to confirm";
    Vector2 hintSize = MeasureTextEx(smallFont, hint, 8, 1);
    DrawTextEx(smallFont, hint,
               (Vector2){ (gameScreenWidth - hintSize.x) / 2, gameScreenHeight - 20 },
               8, 1, WHITE);
}

/* I/O FUNCTIONS */

/* Initialize default scores in memory: CTO, 100..10 descending. */
static void DefaultsHighScores(void) {
    for (int i = 0; i < HIGH_SCORE_COUNT; ++i) {
        highScores[i].name[0] = 'C';
        highScores[i].name[1] = 'T';
        highScores[i].name[2] = 'O';
        highScores[i].name[3] = '\0';
        highScores[i].score = (HIGH_SCORE_COUNT - i) * 10;
    }
}

void LoadHighScores(void) {
    const char* path = "breakout.lst";

    if (!FileExists(path)) {
        // Seed defaults and write a fresh file
        DefaultsHighScores();
        SaveHighScores();
        return;
    }

    // Ensure table has at least 10 entries even if file is shorter
    for (int i = 0; i < HIGH_SCORE_COUNT; ++i) {
        strncpy(highScores[i].name, "---", sizeof(highScores[i].name));
        highScores[i].name[sizeof(highScores[i].name) - 1] = '\0';
        highScores[i].score = 0;
    }

    FILE* f = fopen(path, "r");
    if (!f) { DefaultsHighScores(); return; }

    // File format (Lua style): alternating lines NAME (3 chars) then SCORE
    // Example:
    // CTO
    // 10000
    // ...
    char line[128];
    int index = 0;
    bool expectingName = true;

    while (fgets(line, sizeof(line), f) != NULL && index < HIGH_SCORE_COUNT) {
        // Strip trailing newline
        size_t len = strlen(line);
        if (len && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

        if (expectingName) {
            // Take first 3 chars (pad with '-')
            char n0 = (len > 0) ? line[0] : '-';
            char n1 = (len > 1) ? line[1] : '-';
            char n2 = (len > 2) ? line[2] : '-';
            highScores[index].name[0] = n0;
            highScores[index].name[1] = n1;
            highScores[index].name[2] = n2;
            highScores[index].name[3] = '\0';
        } else {
            highScores[index].score = (int)strtol(line, NULL, 10);
            index++;
        }
        expectingName = !expectingName;
    }

    fclose(f);
}

void SaveHighScores(void) {
    const char* path = "breakout.lst";
    FILE* f = fopen(path, "w");
    if (!f) return;

    // Write as alternating NAME and SCORE lines (matches Lua)
    for (int i = 0; i < HIGH_SCORE_COUNT; ++i) {
        // Always 3 letters (pad with '-')
        char n0 = highScores[i].name[0] ? highScores[i].name[0] : '-';
        char n1 = highScores[i].name[1] ? highScores[i].name[1] : '-';
        char n2 = highScores[i].name[2] ? highScores[i].name[2] : '-';

        fprintf(f, "%c%c%c\n", n0, n1, n2);
        fprintf(f, "%d\n", highScores[i].score);
    }

    fclose(f);
}

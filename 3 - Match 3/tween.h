// tween.h
// Minimal header-only tween/Timer system for Raylib projects.
// Mirrors the idea of Knife.timer's Timer.tween (linear easing).
// - Create a Tween with duration
// - Add properties (float*) with their target values
// - Start it (captures initial values)
// - Call Tween_UpdateAll(dt) each frame
// - Optional: on_finish callback
//
// Notes:
// * Uses a fixed-size pool (no malloc) for predictability.
// * Linear easing only; we'll add easings/sequencing later.
// * All values are float-based (sufficient for positions/alpha/scales).

#ifndef TWEEN_H
#define TWEEN_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Configuration ---
#ifndef TWEEN_MAX_TWEENS
#define TWEEN_MAX_TWEENS 2048
#endif

#ifndef TWEEN_MAX_TASKS_PER_TWEEN
#define TWEEN_MAX_TASKS_PER_TWEEN 8
#endif

typedef float (*TweenEaseFn)(float t, float b, float c, float d);
typedef void  (*TweenFinishFn)(void *user);

// A single property animation: target pointer + (initial, change)
typedef struct {
    float *target;
    float  initial;
    float  change;
} TweenTask;

// A tween animates multiple properties over 'duration'
typedef struct {
    bool   active;
    float  duration;
    float  elapsed;
    TweenEaseFn ease;     // easing function (linear for now)
    TweenFinishFn on_finish;
    void  *on_finish_ud;

    int     task_count;
    TweenTask tasks[TWEEN_MAX_TASKS_PER_TWEEN];
} Tween;

// --- Public API ---

// Call once on startup (resets pool).
void Tween_InitSystem(void);

// Create a tween with given duration (seconds). Returns pointer or NULL if pool full.
Tween* Tween_Create(float duration);

// Add a property to tween from current (*target) to 'final_value'.
// Must be called before Tween_Start.
bool Tween_Add(Tween *tw, float *target, float final_value);

// Optional: set easing function. Defaults to linear.
void Tween_SetEase(Tween *tw, TweenEaseFn ease);

// Optional: set finish callback (called once when tween completes).
void Tween_OnFinish(Tween *tw, TweenFinishFn fn, void *user);

// Start tween: captures initial values and activates it.
bool Tween_Start(Tween *tw);

// Update all tweens in the default group.
void Tween_UpdateAll(float dt);

// Cancel all active tweens (does not change property values).
void Tween_ClearAll(void);

// --- Built-in easing ---
float Tween_EaseLinear(float t, float b, float c, float d);

// --- Implementation ---
#ifdef TWEEN_IMPL

static Tween g_tweenPool[TWEEN_MAX_TWEENS];
static bool  g_initialized = false;

float Tween_EaseLinear(float t, float b, float c, float d) {
    // classic linear: b + c * (t/d)
    if (d <= 0.0f) return b + c;
    return b + (c * (t / d));
}

void Tween_InitSystem(void) {
    for (int i = 0; i < TWEEN_MAX_TWEENS; ++i) {
        g_tweenPool[i].active = false;
        g_tweenPool[i].task_count = 0;
        g_tweenPool[i].on_finish = NULL;
        g_tweenPool[i].on_finish_ud = NULL;
    }
    g_initialized = true;
}

static Tween* Tween_Alloc(void) {
    for (int i = 0; i < TWEEN_MAX_TWEENS; ++i) {
        if (!g_tweenPool[i].active && g_tweenPool[i].task_count == 0) {
            return &g_tweenPool[i];
        }
    }
    return NULL;
}

Tween* Tween_Create(float duration) {
    if (!g_initialized) Tween_InitSystem();
    Tween *tw = Tween_Alloc();
    if (!tw) return NULL;
    tw->duration = (duration > 0.0f) ? duration : 0.0f;
    tw->elapsed  = 0.0f;
    tw->ease     = Tween_EaseLinear;
    tw->on_finish = NULL;
    tw->on_finish_ud = NULL;
    tw->task_count = 0;
    tw->active = false; // becomes true after Start
    return tw;
}

bool Tween_Add(Tween *tw, float *target, float final_value) {
    if (!tw || !target) return false;
    if (tw->task_count >= TWEEN_MAX_TASKS_PER_TWEEN) return false;
    TweenTask *task = &tw->tasks[tw->task_count++];
    task->target = target;
    // initial/change captured in Start(), not here
    task->initial = 0.0f;
    task->change  = final_value; // temporarily store final in 'change' slot
    return true;
}

void Tween_SetEase(Tween *tw, TweenEaseFn ease) {
    if (tw) tw->ease = ease ? ease : Tween_EaseLinear;
}

void Tween_OnFinish(Tween *tw, TweenFinishFn fn, void *user) {
    if (!tw) return;
    tw->on_finish = fn;
    tw->on_finish_ud = user;
}

bool Tween_Start(Tween *tw) {
    if (!tw) return false;
    // Convert stored finals into (initial, change)
    for (int i = 0; i < tw->task_count; ++i) {
        TweenTask *tk = &tw->tasks[i];
        float current = *(tk->target);
        float final   = tk->change;    // previously stored final
        tk->initial   = current;
        tk->change    = final - current;
    }
    tw->elapsed = 0.0f;
    tw->active  = true;
    return true;
}

static void Tween_UpdateOne(Tween *tw, float dt) {
    if (!tw->active) return;
    tw->elapsed += dt;

    if (tw->elapsed >= tw->duration) {
        // Snap to final values
        for (int i = 0; i < tw->task_count; ++i) {
            TweenTask *tk = &tw->tasks[i];
            *(tk->target) = tk->initial + tk->change;
        }
        tw->active = false;
        // Make it reusable after completion
        tw->task_count = 0;
        if (tw->on_finish) tw->on_finish(tw->on_finish_ud);
        return;
    }

    const float t = tw->elapsed;
    const float d = (tw->duration > 0.0f) ? tw->duration : 1.0f;
    for (int i = 0; i < tw->task_count; ++i) {
        TweenTask *tk = &tw->tasks[i];
        *(tk->target) = tw->ease(t, tk->initial, tk->change, d);
    }
}

void Tween_UpdateAll(float dt) {
    if (!g_initialized) return;
    if (dt <= 0.0f) return;
    for (int i = 0; i < TWEEN_MAX_TWEENS; ++i) {
        if (g_tweenPool[i].active) {
            Tween_UpdateOne(&g_tweenPool[i], dt);
        }
    }
}

void Tween_ClearAll(void) {
    if (!g_initialized) return;
    for (int i = 0; i < TWEEN_MAX_TWEENS; ++i) {
        g_tweenPool[i].active = false;
        g_tweenPool[i].task_count = 0;
        g_tweenPool[i].on_finish = NULL;
        g_tweenPool[i].on_finish_ud = NULL;
        g_tweenPool[i].elapsed = 0.0f;
    }
}

#endif // TWEEN_IMPL

#ifdef __cplusplus
}
#endif
#endif // TWEEN_H

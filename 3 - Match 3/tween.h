// tween.h
// Header-only Tween & Timer.after utilities for Raylib projects.
// Stage: Tween — with common easings and "after" scheduler.
// Public API (summary):
//   - Tween_InitSystem(), Tween_UpdateAll(dt), Tween_ClearAll()
//   - Tween_Create(duration), Tween_Add(tw, &var, final), Tween_Start(tw)
//   - Tween_SetEase(tw, fn), Tween_OnFinish(tw, cb, user)
//   - Easing functions: Tween_EaseLinear, Tween_EaseInQuad, Tween_EaseOutQuad,
//                      Tween_EaseInOutQuad, Tween_EaseOutBack
//   - Timer_After(delay, cb, user)   // like Knife.timer.after
//
// Notes:
// * Fixed-size pools (no malloc).
// * All animated properties are float-based.

#ifndef TWEEN_H
#define TWEEN_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------- Configuration ----------
#ifndef TWEEN_MAX_TWEENS
#define TWEEN_MAX_TWEENS 2048
#endif

#ifndef TWEEN_MAX_TASKS_PER_TWEEN
#define TWEEN_MAX_TASKS_PER_TWEEN 8
#endif

#ifndef TIMER_MAX_AFTER_JOBS
#define TIMER_MAX_AFTER_JOBS 1024
#endif

// ---------- Types ----------
typedef float (*TweenEaseFn)(float t, float b, float c, float d);
typedef void  (*TweenFinishFn)(void *user);

// A single property animation: target pointer + (initial, change)
typedef struct {
    float *target;
    float  initial;
    float  change;
} TweenTask;

typedef struct {
    bool   active;
    float  duration;
    float  elapsed;
    TweenEaseFn ease;     // easing function
    TweenFinishFn on_finish;
    void  *on_finish_ud;

    int     task_count;
    TweenTask tasks[TWEEN_MAX_TASKS_PER_TWEEN];
} Tween;

// ---------- Public API (tweening) ----------
void   Tween_InitSystem(void);
Tween* Tween_Create(float duration);
bool   Tween_Add(Tween *tw, float *target, float final_value);
void   Tween_SetEase(Tween *tw, TweenEaseFn ease);
void   Tween_OnFinish(Tween *tw, TweenFinishFn fn, void *user);
bool   Tween_Start(Tween *tw);
void   Tween_UpdateAll(float dt);
void   Tween_ClearAll(void);

// ---------- Easing helpers ----------
float Tween_EaseLinear   (float t, float b, float c, float d);
float Tween_EaseInQuad   (float t, float b, float c, float d);
float Tween_EaseOutQuad  (float t, float b, float c, float d);
float Tween_EaseInOutQuad(float t, float b, float c, float d);
float Tween_EaseOutBack  (float t, float b, float c, float d); // s = 1.70158

// ---------- Timer.after ----------
typedef struct {
    bool  active;
    float remaining;
    TweenFinishFn cb;
    void *ud;
} TimerAfterJob;

// Schedule a callback to fire once after `delay` seconds.
// Returns true on success.
bool Timer_After(float delay, TweenFinishFn cb, void *user);

#ifdef TWEEN_IMPL
// ---------- Implementation ----------

static bool  g_initialized = false;

static Tween g_tweenPool[TWEEN_MAX_TWEENS];
static TimerAfterJob g_afterJobs[TIMER_MAX_AFTER_JOBS];

// ---- Easings ----
float Tween_EaseLinear(float t, float b, float c, float d) {
    if (d <= 0.0f) return b + c;
    return b + c * (t / d);
}
float Tween_EaseInQuad(float t, float b, float c, float d) {
    if (d <= 0.0f) return b + c;
    t /= d; return b + c * t * t;
}
float Tween_EaseOutQuad(float t, float b, float c, float d) {
    if (d <= 0.0f) return b + c;
    t /= d; return b - c * t * (t - 2.0f);
}
float Tween_EaseInOutQuad(float t, float b, float c, float d) {
    if (d <= 0.0f) return b + c;
    t /= (d * 0.5f);
    if (t < 1.0f) return b + (c * 0.5f) * t * t;
    t -= 1.0f;
    return b + (-c * 0.5f) * (t * (t - 2.0f) - 1.0f);
}
float Tween_EaseOutBack(float t, float b, float c, float d) {
    if (d <= 0.0f) return b + c;
    const float s = 1.70158f;
    t = t / d - 1.0f;
    return b + c * (t * t * ((s + 1.0f) * t + s) + 1.0f);
}

// ---- System init/clear ----
void Tween_InitSystem(void) {
    for (int i = 0; i < TWEEN_MAX_TWEENS; ++i) {
        g_tweenPool[i].active = false;
        g_tweenPool[i].task_count = 0;
        g_tweenPool[i].on_finish = NULL;
        g_tweenPool[i].on_finish_ud = NULL;
        g_tweenPool[i].elapsed = 0.0f;
        g_tweenPool[i].duration = 0.0f;
        g_tweenPool[i].ease = Tween_EaseLinear;
    }
    for (int j = 0; j < TIMER_MAX_AFTER_JOBS; ++j) {
        g_afterJobs[j].active = false;
        g_afterJobs[j].remaining = 0.0f;
        g_afterJobs[j].cb = NULL;
        g_afterJobs[j].ud = NULL;
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
    // We'll capture initial in Start(); temporarily store final in 'change'
    task->initial = 0.0f;
    task->change  = final_value;
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
    for (int i = 0; i < tw->task_count; ++i) {
        TweenTask *tk = &tw->tasks[i];
        float current = *(tk->target);
        float final   = tk->change;    // stored final
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
        // Snap to finals
        for (int i = 0; i < tw->task_count; ++i) {
            TweenTask *tk = &tw->tasks[i];
            *(tk->target) = tk->initial + tk->change;
        }
        tw->active = false;
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

// ---- Timer.after ----
static bool Timer_After_Alloc(float delay, TweenFinishFn cb, void *user) {
    if (delay < 0.0f) delay = 0.0f;
    for (int i = 0; i < TIMER_MAX_AFTER_JOBS; ++i) {
        if (!g_afterJobs[i].active) {
            g_afterJobs[i].active    = true;
            g_afterJobs[i].remaining = delay;
            g_afterJobs[i].cb        = cb;
            g_afterJobs[i].ud        = user;
            return true;
        }
    }
    return false;
}

bool Timer_After(float delay, TweenFinishFn cb, void *user) {
    if (!g_initialized) Tween_InitSystem();
    if (!cb) return false;
    return Timer_After_Alloc(delay, cb, user);
}

static void Timer_After_Update(float dt) {
    for (int i = 0; i < TIMER_MAX_AFTER_JOBS; ++i) {
        if (!g_afterJobs[i].active) continue;
        g_afterJobs[i].remaining -= dt;
        if (g_afterJobs[i].remaining <= 0.0f) {
            TweenFinishFn cb = g_afterJobs[i].cb;
            void *ud         = g_afterJobs[i].ud;
            g_afterJobs[i].active = false;
            g_afterJobs[i].cb = NULL;
            g_afterJobs[i].ud = NULL;
            if (cb) cb(ud);
        }
    }
}

// ---- Update/Clear ----
void Tween_UpdateAll(float dt) {
    if (!g_initialized) return;
    if (dt <= 0.0f) return;

    // Update tweens
    for (int i = 0; i < TWEEN_MAX_TWEENS; ++i) {
        if (g_tweenPool[i].active) {
            Tween_UpdateOne(&g_tweenPool[i], dt);
        }
    }
    // Update Timer.after jobs
    Timer_After_Update(dt);
}

void Tween_ClearAll(void) {
    if (!g_initialized) return;
    for (int i = 0; i < TWEEN_MAX_TWEENS; ++i) {
        g_tweenPool[i].active = false;
        g_tweenPool[i].task_count = 0;
        g_tweenPool[i].on_finish = NULL;
        g_tweenPool[i].on_finish_ud = NULL;
        g_tweenPool[i].elapsed = 0.0f;
        g_tweenPool[i].duration = 0.0f;
        g_tweenPool[i].ease = Tween_EaseLinear;
    }
    for (int j = 0; j < TIMER_MAX_AFTER_JOBS; ++j) {
        g_afterJobs[j].active = false;
        g_afterJobs[j].remaining = 0.0f;
        g_afterJobs[j].cb = NULL;
        g_afterJobs[j].ud = NULL;
    }
}

#endif // TWEEN_IMPL

#ifdef __cplusplus
}
#endif
#endif // TWEEN_H

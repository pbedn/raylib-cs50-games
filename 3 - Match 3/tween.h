// tween.h
// Tween + Timer.after + Chain (sequence) for Raylib projects.
// Fixed-pool, header-only, practical API close to CS50's knife.timer usage.

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

#ifndef CHAIN_MAX_CHAINS
#define CHAIN_MAX_CHAINS 1024
#endif

#ifndef CHAIN_MAX_STEPS_PER_CHAIN
#define CHAIN_MAX_STEPS_PER_CHAIN 32
#endif

#ifndef CHAIN_MAX_PROPS_PER_TWEEN_STEP
#define CHAIN_MAX_PROPS_PER_TWEEN_STEP 8
#endif

// ---------- Core types ----------
typedef float (*TweenEaseFn)(float t, float b, float c, float d);
typedef void  (*TweenFinishFn)(void *user);

typedef struct {
    float *target;
    float  initial;
    float  change;
} TweenTask;

typedef struct {
    bool   active;
    float  duration;
    float  elapsed;
    TweenEaseFn ease;
    TweenFinishFn on_finish;
    void  *on_finish_ud;

    int     task_count;
    TweenTask tasks[TWEEN_MAX_TASKS_PER_TWEEN];
} Tween;

// ---------- Tween API ----------
void   Tween_InitSystem(void);
Tween* Tween_Create(float duration);
bool   Tween_Add(Tween *tw, float *target, float final_value);
void   Tween_SetEase(Tween *tw, TweenEaseFn ease);
void   Tween_OnFinish(Tween *tw, TweenFinishFn fn, void *user);
bool   Tween_Start(Tween *tw);
void   Tween_UpdateAll(float dt);
void   Tween_ClearAll(void);

// ---------- Easings ----------
float Tween_EaseLinear   (float t, float b, float c, float d);
float Tween_EaseInQuad   (float t, float b, float c, float d);
float Tween_EaseOutQuad  (float t, float b, float c, float d);
float Tween_EaseInOutQuad(float t, float b, float c, float d);
float Tween_EaseOutBack  (float t, float b, float c, float d); // s=1.70158

// ---------- Timer.after ----------
typedef struct {
    bool  active;
    float remaining;
    TweenFinishFn cb;
    void *ud;
} TimerAfterJob;

bool Timer_After(float delay, TweenFinishFn cb, void *user);

// ---------- Chain (sequence) ----------
typedef struct Chain Chain;

// Build steps before starting:
Chain* Chain_Create(void);
bool   Chain_TweenBegin(Chain *ch, float duration);           // begin a tween step
bool   Chain_TweenAdd  (Chain *ch, float *target, float final_value);
void   Chain_TweenSetEase(Chain *ch, TweenEaseFn ease);       // optional; defaults to linear
bool   Chain_TweenEnd  (Chain *ch);                           // finalize tween step
bool   Chain_Delay     (Chain *ch, float seconds);            // add delay step
bool   Chain_Call      (Chain *ch, TweenFinishFn cb, void *ud); // add immediate call step
void   Chain_OnFinish  (Chain *ch, TweenFinishFn cb, void *ud);
bool   Chain_Start     (Chain *ch);                           // start executing the chain

#ifdef TWEEN_IMPL
// ================== Implementation ==================
#include <string.h>

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
    tw->active = false;
    return tw;
}

bool Tween_Add(Tween *tw, float *target, float final_value) {
    if (!tw || !target) return false;
    if (tw->task_count >= TWEEN_MAX_TASKS_PER_TWEEN) return false;
    TweenTask *task = &tw->tasks[tw->task_count++];
    task->target = target;
    // Store final temporarily in 'change'; capture on Start()
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

// ---- Chain implementation ----
typedef enum {
    CHAIN_STEP_NONE = 0,
    CHAIN_STEP_TWEEN_DEF,
    CHAIN_STEP_DELAY,
    CHAIN_STEP_CALL
} ChainStepType;

typedef struct {
    float *target;
    float  final_value;
} ChainTweenPropDef;

typedef struct {
    float duration;
    TweenEaseFn ease;
    int   prop_count;
    ChainTweenPropDef props[CHAIN_MAX_PROPS_PER_TWEEN_STEP];
    // runtime
    Tween *runtime; // created at step start; NULL until then
} ChainTweenDef;

typedef struct {
    ChainStepType type;
    union {
        ChainTweenDef tween;
        struct { float remaining; } delay;
        struct { TweenFinishFn cb; void *ud; } call;
    };
} ChainStep;

struct Chain {
    bool active;
    bool building_tween; // true between TweenBegin/TweenEnd
    int  step_count;
    int  current;        // current step index
    TweenFinishFn on_finish;
    void *on_finish_ud;
    ChainStep steps[CHAIN_MAX_STEPS_PER_CHAIN];
};

static Chain g_chainPool[CHAIN_MAX_CHAINS];

static void Chain_Reset(Chain *ch) {
    ch->active = false;
    ch->building_tween = false;
    ch->step_count = 0;
    ch->current = 0;
    ch->on_finish = NULL;
    ch->on_finish_ud = NULL;
    for (int i = 0; i < CHAIN_MAX_STEPS_PER_CHAIN; ++i) {
        ch->steps[i].type = CHAIN_STEP_NONE;
    }
}

static Chain* Chain_Alloc(void) {
    for (int i = 0; i < CHAIN_MAX_CHAINS; ++i) {
        if (!g_chainPool[i].active && g_chainPool[i].step_count == 0 && !g_chainPool[i].building_tween) {
            Chain_Reset(&g_chainPool[i]);
            return &g_chainPool[i];
        }
    }
    return NULL;
}

Chain* Chain_Create(void) {
    if (!g_initialized) Tween_InitSystem();
    return Chain_Alloc();
}

bool Chain_TweenBegin(Chain *ch, float duration) {
    if (!ch || ch->active || ch->building_tween) return false;
    if (ch->step_count >= CHAIN_MAX_STEPS_PER_CHAIN) return false;
    ChainStep *st = &ch->steps[ch->step_count++];
    st->type = CHAIN_STEP_TWEEN_DEF;
    st->tween.duration = (duration > 0.0f) ? duration : 0.0f;
    st->tween.ease = Tween_EaseLinear;
    st->tween.prop_count = 0;
    st->tween.runtime = NULL;
    ch->building_tween = true;
    return true;
}

bool Chain_TweenAdd(Chain *ch, float *target, float final_value) {
    if (!ch || !ch->building_tween) return false;
    ChainStep *st = &ch->steps[ch->step_count - 1];
    if (st->type != CHAIN_STEP_TWEEN_DEF) return false;
    if (st->tween.prop_count >= CHAIN_MAX_PROPS_PER_TWEEN_STEP) return false;
    st->tween.props[st->tween.prop_count++] = (ChainTweenPropDef){ target, final_value };
    return true;
}

void Chain_TweenSetEase(Chain *ch, TweenEaseFn ease) {
    if (!ch || !ch->building_tween) return;
    ChainStep *st = &ch->steps[ch->step_count - 1];
    if (st->type != CHAIN_STEP_TWEEN_DEF) return;
    st->tween.ease = ease ? ease : Tween_EaseLinear;
}

bool Chain_TweenEnd(Chain *ch) {
    if (!ch || !ch->building_tween) return false;
    ch->building_tween = false;
    return true;
}

bool Chain_Delay(Chain *ch, float seconds) {
    if (!ch || ch->active || ch->building_tween) return false;
    if (ch->step_count >= CHAIN_MAX_STEPS_PER_CHAIN) return false;
    if (seconds < 0.0f) seconds = 0.0f;
    ChainStep *st = &ch->steps[ch->step_count++];
    st->type = CHAIN_STEP_DELAY;
    st->delay.remaining = seconds;
    return true;
}

bool Chain_Call(Chain *ch, TweenFinishFn cb, void *ud) {
    if (!ch || ch->active || ch->building_tween) return false;
    if (ch->step_count >= CHAIN_MAX_STEPS_PER_CHAIN) return false;
    ChainStep *st = &ch->steps[ch->step_count++];
    st->type = CHAIN_STEP_CALL;
    st->call.cb = cb;
    st->call.ud = ud;
    return true;
}

void Chain_OnFinish(Chain *ch, TweenFinishFn cb, void *ud) {
    if (!ch) return;
    ch->on_finish = cb;
    ch->on_finish_ud = ud;
}

bool Chain_Start(Chain *ch) {
    if (!ch || ch->active || ch->building_tween) return false;
    ch->active = true;
    ch->current = 0;
    return true;
}

static bool Chain_StartTweenRuntime(ChainStep *st) {
    // Create and start a runtime Tween from the stored definition
    Tween *tw = Tween_Create(st->tween.duration);
    if (!tw) return false;
    for (int i = 0; i < st->tween.prop_count; ++i) {
        ChainTweenPropDef *p = &st->tween.props[i];
        if (!Tween_Add(tw, p->target, p->final_value)) return false;
    }
    Tween_SetEase(tw, st->tween.ease);
    Tween_Start(tw);
    st->tween.runtime = tw;
    return true;
}

static void Chain_UpdateOne(Chain *ch, float dt) {
    if (!ch->active) return;

    // Progress through immediate steps (CALL) within the same frame.
    while (ch->active && ch->current < ch->step_count) {
        ChainStep *st = &ch->steps[ch->current];

        switch (st->type) {
            case CHAIN_STEP_CALL: {
                TweenFinishFn cb = st->call.cb;
                void *ud = st->call.ud;
                // Consume this step immediately
                ch->current++;
                if (cb) cb(ud);
            } break;

            case CHAIN_STEP_DELAY: {
                st->delay.remaining -= dt;
                if (st->delay.remaining <= 0.0f) {
                    ch->current++;
                    // continue loop to process next step this frame
                } else {
                    return; // still delaying
                }
            } break;

            case CHAIN_STEP_TWEEN_DEF: {
                // If runtime tween not created yet, create+start once.
                if (!st->tween.runtime) {
                    if (!Chain_StartTweenRuntime(st)) {
                        // Failed to allocate tween; skip step to avoid deadlock.
                        ch->current++;
                        break;
                    }
                }
                // Wait until tween completes (Tween_UpdateAll will toggle active=false)
                if (st->tween.runtime->active) {
                    return; // still running
                } else {
                    // tween completed, advance
                    st->tween.runtime = NULL; // allow reuse of pool slot
                    ch->current++;
                    // continue loop to process next step this frame
                }
            } break;

            default:
                ch->current++;
                break;
        }
    }

    if (ch->current >= ch->step_count) {
        ch->active = false;
        // Reset steps so chain can be rebuilt or reused
        int had_steps = ch->step_count;
        TweenFinishFn cb = ch->on_finish;
        void *ud = ch->on_finish_ud;
        Chain_Reset(ch); // clears steps
        if (cb) cb(ud);
        (void)had_steps;
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

    // Update chains
    for (int c = 0; c < CHAIN_MAX_CHAINS; ++c) {
        if (g_chainPool[c].active) {
            Chain_UpdateOne(&g_chainPool[c], dt);
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
        g_tweenPool[i].duration = 0.0f;
        g_tweenPool[i].ease = Tween_EaseLinear;
    }
    for (int j = 0; j < TIMER_MAX_AFTER_JOBS; ++j) {
        g_afterJobs[j].active = false;
        g_afterJobs[j].remaining = 0.0f;
        g_afterJobs[j].cb = NULL;
        g_afterJobs[j].ud = NULL;
    }
    for (int c = 0; c < CHAIN_MAX_CHAINS; ++c) {
        Chain_Reset(&g_chainPool[c]);
    }
}

#endif // TWEEN_IMPL

#ifdef __cplusplus
}
#endif
#endif // TWEEN_H

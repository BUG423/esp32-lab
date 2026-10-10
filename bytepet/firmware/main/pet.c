/**
 * bytepet - pet state machine implementation
 */
#include "pet.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "pet";

/* ---- tuning (seconds of virtual time per unit) ---- */
#define PET_TICK_PERIOD_MS      CONFIG_BYTEPET_TICK_PERIOD_MS
#define PET_TICKS_PER_MINUTE    (60000 / PET_TICK_PERIOD_MS)

/* stage boundaries in minutes */
#define PET_MINUTES_TO_BABY     1       /* 1 minute (or first feed) */
#define PET_MINUTES_TO_TEEN     60      /* 1 hour */
#define PET_MINUTES_TO_ADULT    180     /* 3 hours */
#define PET_MINUTES_TO_GHOST    2880    /* 2 days of neglect */

/* per-minute drift */
#define PET_HUNGER_PER_MIN      1
#define PET_CLEAN_PER_MIN       1
#define PET_ENERGY_PER_MIN      1
#define PET_HAPPY_PER_MIN       1

/* action deltas */
#define PET_FEED_HUNGER         -25
#define PET_FEED_CLEAN          -3
#define PET_FEED_HAPPY          +3
#define PET_PLAY_HAPPY          +20
#define PET_PLAY_ENERGY         -15
#define PET_PLAY_HUNGER         +8
#define PET_CLEAN_SET           100
#define PET_SLEEP_ENERGY_PER_MIN 8

static SemaphoreHandle_t s_lock;
static pet_state_t s_state;
static char s_event[128];

static const char *s_stage_names[PET_STAGE_MAX] = {
    "蛋", "幼年", "少年", "成年", "幽灵"
};

static const char *s_emotion_names[PET_EMOTION_MAX] = {
    "开心", "饿了", "累了", "脏了", "难过", "生病", "睡觉", "幽灵"
};

static const char *s_emotion_emoji[PET_EMOTION_MAX] = {
    "😊", "🍚", "😴", "🧼", "😢", "🤒", "💤", "👻"
};

static const char *s_emotion_colors[PET_EMOTION_MAX] = {
    "#7ee8c7", "#ffb4c8", "#c5b4ff", "#a8e6ff", "#8aa0c8", "#ff8fa3", "#9b8cff", "#8a9bb0"
};

static const char *s_emotion_moods[PET_EMOTION_MAX] = {
    "今天心情超棒，想和你一起玩！",
    "肚子咕咕叫啦，快喂我吃点东西～",
    "眼皮在打架，需要睡一觉……",
    "身上脏兮兮的，帮我洗个澡吧。",
    "好久没理我了，有点小难过……",
    "感觉不太舒服，要多休息哦。",
    "呼……睡得好香，别吵我。",
    "我已经变成幽灵了……喂我三次就能复活。"
};

static const char *s_action_logs[PET_ACTION_MAX] = {
    "小帕吃了东西，好开心！",
    "小帕玩得好尽兴，累并快乐着！",
    "小帕洗得干干净净，香喷喷～",
    "小帕钻进被窝，准备做个美梦。"
};

/* ---- helpers ---- */

static void pet_set_event(const char *msg)
{
    strncpy(s_event, msg, sizeof(s_event) - 1);
    s_event[sizeof(s_event) - 1] = '\0';
}

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static pet_emotion_t pet_derive_emotion(const pet_state_t *s)
{
    if (s->stage == PET_STAGE_GHOST) {
        return PET_EMOTION_GHOST;
    }
    if (s->sleeping) {
        return PET_EMOTION_SLEEPING;
    }
    if (s->hunger >= 85) {
        return PET_EMOTION_HUNGRY;
    }
    if (s->cleanliness <= 15) {
        return PET_EMOTION_DIRTY;
    }
    if (s->energy <= 15) {
        return PET_EMOTION_TIRED;
    }
    if (s->hunger >= 60 || s->energy <= 35 || s->cleanliness <= 35) {
        return PET_EMOTION_SICK;
    }
    if (s->happiness <= 25) {
        return PET_EMOTION_SAD;
    }
    return PET_EMOTION_HAPPY;
}

/* ---- persistence ---- */

static esp_err_t pet_save_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("bytepet", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    nvs_set_i32(h, "hunger", s_state.hunger);
    nvs_set_i32(h, "happiness", s_state.happiness);
    nvs_set_i32(h, "energy", s_state.energy);
    nvs_set_i32(h, "clean", s_state.cleanliness);
    nvs_set_i32(h, "tick", s_state.tick);
    nvs_set_i32(h, "stage", (int32_t)s_state.stage);
    nvs_set_i32(h, "sleep", s_state.sleeping ? 1 : 0);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t pet_load_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("bytepet", NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;
    }
    int32_t v;
    if (nvs_get_i32(h, "hunger", &v) == ESP_OK) s_state.hunger = v;
    if (nvs_get_i32(h, "happiness", &v) == ESP_OK) s_state.happiness = v;
    if (nvs_get_i32(h, "energy", &v) == ESP_OK) s_state.energy = v;
    if (nvs_get_i32(h, "clean", &v) == ESP_OK) s_state.cleanliness = v;
    if (nvs_get_i32(h, "tick", &v) == ESP_OK) s_state.tick = v;
    if (nvs_get_i32(h, "stage", &v) == ESP_OK) s_state.stage = (pet_stage_t)v;
    if (nvs_get_i32(h, "sleep", &v) == ESP_OK) s_state.sleeping = (v != 0);
    nvs_close(h);
    return ESP_OK;
}

/* ---- tick task ---- */

static void pet_apply_minute_drift(void)
{
    int32_t hunger_rate = PET_HUNGER_PER_MIN;
    int32_t clean_rate = PET_CLEAN_PER_MIN;
    int32_t energy_rate = PET_ENERGY_PER_MIN;
    int32_t happy_rate = PET_HAPPY_PER_MIN;

    if (s_state.sleeping) {
        s_state.energy = clamp32(s_state.energy + PET_SLEEP_ENERGY_PER_MIN, 0, 100);
        hunger_rate = PET_HUNGER_PER_MIN / 2;
        clean_rate = PET_CLEAN_PER_MIN / 2;
        happy_rate = 0;
        if (s_state.energy >= 100) {
            s_state.sleeping = false;
            pet_set_event("小帕睡醒啦，精神饱满！");
        }
    } else {
        s_state.hunger = clamp32(s_state.hunger + hunger_rate, 0, 100);
        s_state.energy = clamp32(s_state.energy - energy_rate, 0, 100);
    }
    s_state.cleanliness = clamp32(s_state.cleanliness - clean_rate, 0, 100);

    /* happiness drifts toward a mid value, degraded by bad stats */
    int32_t target = 55;
    if (s_state.hunger > 60) target -= 25;
    if (s_state.energy < 30) target -= 20;
    if (s_state.cleanliness < 30) target -= 15;
    if (s_state.sleeping) target = 55;

    if (s_state.happiness < target) {
        s_state.happiness = clamp32(s_state.happiness + happy_rate, 0, 100);
    } else if (s_state.happiness > target) {
        s_state.happiness = clamp32(s_state.happiness - happy_rate / 2, 0, 100);
    }
}

static void pet_check_evolution(void)
{
    int32_t minutes = s_state.tick / PET_TICKS_PER_MINUTE;

    switch (s_state.stage) {
    case PET_STAGE_EGG:
        if (minutes >= PET_MINUTES_TO_BABY) {
            s_state.stage = PET_STAGE_BABY;
            pet_set_event("啪！蛋壳裂开了，小帕出生啦！");
        }
        break;
    case PET_STAGE_BABY:
        if (minutes >= PET_MINUTES_TO_TEEN) {
            s_state.stage = PET_STAGE_TEEN;
            pet_set_event("小帕长大了，变成少年！");
        }
        break;
    case PET_STAGE_TEEN:
        if (minutes >= PET_MINUTES_TO_ADULT) {
            s_state.stage = PET_STAGE_ADULT;
            pet_set_event("小帕成年啦，越来越有范儿！");
        }
        break;
    case PET_STAGE_ADULT:
        /* neglect for 2 days -> ghost */
        if (s_state.hunger >= 100 && s_state.happiness <= 5 &&
            minutes >= PET_MINUTES_TO_GHOST) {
            s_state.stage = PET_STAGE_GHOST;
            pet_set_event("小帕太孤独了，变成了幽灵……");
        }
        break;
    case PET_STAGE_GHOST:
    default:
        break;
    }
}

static void pet_tick_task(void *arg)
{
    (void)arg;
    int64_t last = esp_timer_get_time();
    int32_t ms_accum = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(PET_TICK_PERIOD_MS));
        int64_t now = esp_timer_get_time();
        ms_accum += (int32_t)((now - last) / 1000);
        last = now;

        /* one "minute" of virtual time */
        if (ms_accum < 60000) {
            continue;
        }
        ms_accum -= 60000;

        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        s_state.tick++;
        s_state.age_minutes = s_state.tick / PET_TICKS_PER_MINUTE;

        if (s_state.stage != PET_STAGE_GHOST) {
            pet_apply_minute_drift();
        }
        s_state.emotion = pet_derive_emotion(&s_state);
        pet_check_evolution();
        s_state.emotion = pet_derive_emotion(&s_state);
        s_state.alive = (s_state.stage != PET_STAGE_GHOST);

        pet_save_nvs();
        xSemaphoreGive(s_lock);
    }
}

/* ---- public API ---- */

esp_err_t pet_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_state, 0, sizeof(s_state));
    s_state.stage = PET_STAGE_EGG;
    s_state.hunger = 30;
    s_state.happiness = 60;
    s_state.energy = 80;
    s_state.cleanliness = 90;
    s_state.tick = 0;
    s_state.alive = true;
    pet_set_event("一颗神秘的蛋来到了这个世界……");

    if (pet_load_nvs() == ESP_OK) {
        ESP_LOGI(TAG, "loaded pet from NVS: stage=%s tick=%d",
                 s_stage_names[s_state.stage], (int)s_state.tick);
    } else {
        ESP_LOGI(TAG, "no saved pet, starting a fresh egg");
        pet_save_nvs();
    }
    s_state.age_minutes = s_state.tick / PET_TICKS_PER_MINUTE;
    s_state.emotion = pet_derive_emotion(&s_state);

    BaseType_t ok = xTaskCreate(pet_tick_task, "pet_tick", 3072, NULL, 5, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "pet initialised");
    return ESP_OK;
}

void pet_get_state(pet_state_t *out)
{
    if (out == NULL) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_state;
    xSemaphoreGive(s_lock);
}

esp_err_t pet_do_action(pet_action_t action)
{
    if (action >= PET_ACTION_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    if (s_state.stage == PET_STAGE_EGG && action == PET_ACTION_FEED) {
        s_state.stage = PET_STAGE_BABY;
        pet_set_event("小帕破壳而出，第一次吃到东西！");
    }

    switch (action) {
    case PET_ACTION_FEED:
        if (s_state.stage == PET_STAGE_GHOST) {
            /* revive counter: feeding 3 times brings the pet back */
            s_state.hunger = clamp32(s_state.hunger + PET_FEED_HUNGER, 0, 100);
            s_state.happiness = clamp32(s_state.happiness + 8, 0, 100);
            s_state.tick += 0;
            static int s_revive_count = 0;
            s_revive_count++;
            if (s_revive_count >= 3) {
                s_state.stage = PET_STAGE_ADULT;
                s_state.hunger = 40;
                s_state.happiness = 50;
                s_state.energy = 50;
                s_state.cleanliness = 50;
                s_revive_count = 0;
                pet_set_event("奇迹发生了！小帕复活啦！");
            } else {
                char buf[64];
                snprintf(buf, sizeof(buf), "幽灵小帕吃了供品 (%d/3)……", s_revive_count);
                pet_set_event(buf);
            }
        } else {
            s_state.hunger = clamp32(s_state.hunger + PET_FEED_HUNGER, 0, 100);
            s_state.cleanliness = clamp32(s_state.cleanliness + PET_FEED_CLEAN, 0, 100);
            s_state.happiness = clamp32(s_state.happiness + PET_FEED_HAPPY, 0, 100);
            pet_set_event(s_action_logs[PET_ACTION_FEED]);
        }
        break;

    case PET_ACTION_PLAY:
        if (s_state.stage == PET_STAGE_GHOST) {
            pet_set_event("幽灵小帕飘来飘去，没有心情玩……");
        } else {
            s_state.happiness = clamp32(s_state.happiness + PET_PLAY_HAPPY, 0, 100);
            s_state.energy = clamp32(s_state.energy + PET_PLAY_ENERGY, 0, 100);
            s_state.hunger = clamp32(s_state.hunger + PET_PLAY_HUNGER, 0, 100);
            pet_set_event(s_action_logs[PET_ACTION_PLAY]);
        }
        break;

    case PET_ACTION_CLEAN:
        if (s_state.stage == PET_STAGE_GHOST) {
            pet_set_event("幽灵小帕不需要洗澡啦。");
        } else {
            s_state.cleanliness = PET_CLEAN_SET;
            pet_set_event(s_action_logs[PET_ACTION_CLEAN]);
        }
        break;

    case PET_ACTION_SLEEP:
        if (s_state.stage == PET_STAGE_GHOST) {
            pet_set_event("幽灵小帕已经永远安息了……");
        } else {
            s_state.sleeping = !s_state.sleeping;
            if (s_state.sleeping) {
                pet_set_event(s_action_logs[PET_ACTION_SLEEP]);
            } else {
                pet_set_event("小帕揉揉眼睛醒了过来。");
            }
        }
        break;

    default:
        break;
    }

    s_state.emotion = pet_derive_emotion(&s_state);
    s_state.alive = (s_state.stage != PET_STAGE_GHOST);
    pet_save_nvs();
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

int pet_get_event_log(char *buf, size_t buflen)
{
    if (buf == NULL || buflen == 0) {
        return 0;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    strncpy(buf, s_event, buflen - 1);
    buf[buflen - 1] = '\0';
    xSemaphoreGive(s_lock);
    return (int)strlen(buf);
}

const char *pet_stage_name(pet_stage_t stage)
{
    if (stage >= PET_STAGE_MAX) {
        return "?";
    }
    return s_stage_names[stage];
}

const char *pet_emotion_name(pet_emotion_t emotion)
{
    if (emotion >= PET_EMOTION_MAX) {
        return "?";
    }
    return s_emotion_names[emotion];
}

const char *pet_emotion_emoji(pet_emotion_t emotion)
{
    if (emotion >= PET_EMOTION_MAX) {
        return "❓";
    }
    return s_emotion_emoji[emotion];
}

const char *pet_emotion_color(pet_emotion_t emotion)
{
    if (emotion >= PET_EMOTION_MAX) {
        return "#ffffff";
    }
    return s_emotion_colors[emotion];
}

const char *pet_emotion_mood(pet_emotion_t emotion)
{
    if (emotion >= PET_EMOTION_MAX) {
        return "...";
    }
    return s_emotion_moods[emotion];
}

/**
 * bytepet - virtual pet state machine, stats and evolution
 *
 * Stats live in RAM and are mirrored to NVS so the pet survives reboot.
 * A FreeRTOS tick task ages the pet and decays stats over time.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Pet life-cycle stages. */
typedef enum {
    PET_STAGE_EGG = 0,
    PET_STAGE_BABY,
    PET_STAGE_TEEN,
    PET_STAGE_ADULT,
    PET_STAGE_GHOST,
    PET_STAGE_MAX
} pet_stage_t;

/** Derived emotional state (from stats + flags). */
typedef enum {
    PET_EMOTION_HAPPY = 0,
    PET_EMOTION_HUNGRY,
    PET_EMOTION_TIRED,
    PET_EMOTION_DIRTY,
    PET_EMOTION_SAD,
    PET_EMOTION_SICK,
    PET_EMOTION_SLEEPING,
    PET_EMOTION_GHOST,
    PET_EMOTION_MAX
} pet_emotion_t;

/** Actions the user can trigger. */
typedef enum {
    PET_ACTION_FEED = 0,
    PET_ACTION_PLAY,
    PET_ACTION_CLEAN,
    PET_ACTION_SLEEP,
    PET_ACTION_MAX
} pet_action_t;

/** Full snapshot used by the web server / button layer. */
typedef struct {
    pet_stage_t   stage;
    pet_emotion_t emotion;
    int32_t       hunger;       /* 0 = full, 100 = starving */
    int32_t       happiness;    /* 0 = miserable, 100 = delighted */
    int32_t       energy;       /* 0 = exhausted, 100 = rested */
    int32_t       cleanliness;  /* 0 = filthy, 100 = spotless */
    int32_t       tick;         /* total ticks since birth */
    int32_t       age_minutes;  /* derived from tick */
    bool          sleeping;
    bool          alive;
} pet_state_t;

/**
 * @brief Initialise the pet: load from NVS or create a fresh egg.
 *        Starts the internal tick task.
 */
esp_err_t pet_init(void);

/** @brief Snapshot current state (thread-safe). */
void pet_get_state(pet_state_t *out);

/** @brief Apply a user action. Logs an event line. */
esp_err_t pet_do_action(pet_action_t action);

/**
 * @brief Format the latest event string into buf (UTF-8 Chinese).
 * @return length written.
 */
int pet_get_event_log(char *buf, size_t buflen);

/** @brief Human-readable stage name (UTF-8). */
const char *pet_stage_name(pet_stage_t stage);

/** @brief Human-readable emotion name (UTF-8). */
const char *pet_emotion_name(pet_emotion_t emotion);

/** @brief Emoji glyph for an emotion (static string). */
const char *pet_emotion_emoji(pet_emotion_t emotion);

/** @brief CSS colour (hex) for an emotion. */
const char *pet_emotion_color(pet_emotion_t emotion);

/** @brief Short Chinese mood line for an emotion. */
const char *pet_emotion_mood(pet_emotion_t emotion);

#ifdef __cplusplus
}
#endif

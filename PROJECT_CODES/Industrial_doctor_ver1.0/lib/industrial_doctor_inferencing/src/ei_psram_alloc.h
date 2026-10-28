#ifndef EI_PSRAM_ALLOC_H
#define EI_PSRAM_ALLOC_H

// =============================================================================
//  ei_psram_alloc.h — PSRAM allocator override for Edge Impulse on ESP32-S3
//
//  PLACE AT:
//    lib/industrial_doctor_inferencing/src/ei_psram_alloc.h
//
//  COMPANION FILE (required):
//    lib/industrial_doctor_inferencing/src/ei_psram_alloc.cpp
//
//  WHY A SEPARATE .cpp IS REQUIRED:
//  Edge Impulse defines ei_malloc / ei_calloc / ei_free as __attribute__((weak))
//  in ei_classifier_porting.cpp. To override a weak symbol the linker needs a
//  STRONG symbol coming from a compiled object (.o) file.
//  An 'inline' function in a header does NOT produce a strong object-file symbol —
//  the override silently fails and internal RAM keeps getting used.
//  Defining them as normal functions in ei_psram_alloc.cpp fixes this.
//
//  INCLUDE ORDER IN main.cpp  (order matters!):
//    1. #include "ei_psram_alloc.h"
//    2. #include <industrial_doctor_inferencing.h>
// =============================================================================

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Strong symbol declarations — implemented in ei_psram_alloc.cpp
// These override the __attribute__((weak)) versions in ei_classifier_porting.cpp
void* ei_malloc(size_t size);
void* ei_calloc(size_t nitems, size_t size);
void  ei_free(void* ptr);

#ifdef __cplusplus
}
#endif

// -----------------------------------------------------------------------------
//  Debug helper — inline so it doesn't need its own .cpp
//  Call in setup() after psramInit() to confirm memory before inference
// -----------------------------------------------------------------------------
#ifdef ARDUINO
#include <Arduino.h>
#include "esp_heap_caps.h"

inline void ei_psram_report() {
    Serial.println(F("=== Memory Report ==="));
    Serial.printf("  Internal Free Heap : %6u bytes\n", (unsigned)ESP.getFreeHeap());
    Serial.printf("  Internal Min Heap  : %6u bytes\n", (unsigned)ESP.getMinFreeHeap());
    Serial.printf("  PSRAM Total        : %6u bytes\n", (unsigned)ESP.getPsramSize());
    Serial.printf("  PSRAM Free         : %6u bytes\n", (unsigned)ESP.getFreePsram());
    if (ESP.getPsramSize() == 0) {
        Serial.println(F("  *** PSRAM NOT FOUND — check board config! ***"));
        Serial.println(F("      Flags needed : BOARD_HAS_PSRAM, CONFIG_SPIRAM_MODE_OCT=1"));
        Serial.println(F("      ini needed   : board_build.arduino.memory_type = qio_opi"));
    } else {
        Serial.println(F("  PSRAM: OK"));
    }
    Serial.println(F("====================="));
}
#endif // ARDUINO

#endif // EI_PSRAM_ALLOC_H
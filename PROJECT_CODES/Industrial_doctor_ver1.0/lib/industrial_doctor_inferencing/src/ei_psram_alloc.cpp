// =============================================================================
//  ei_psram_alloc.cpp — Strong symbol PSRAM allocator overrides
//
//  PLACE AT:
//    lib/industrial_doctor_inferencing/src/ei_psram_alloc.cpp
//
//  HOW THE OVERRIDE WORKS:
//  Edge Impulse's ei_classifier_porting.cpp defines ei_malloc/calloc/free as
//  __attribute__((weak)). The linker picks the STRONG symbol over the weak one
//  when both are present. This file provides those strong symbols so all Edge
//  Impulse allocations prefer PSRAM, falling back to internal RAM if PSRAM is
//  full or unavailable.
//
//  IMPORTANT — ARENA SIZE NOTE:
//  Your model_metadata.h shows:
//    EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE = 14214  (~14 KB)
//  This is very small and fits easily in internal RAM. The crash you were
//  seeing was NOT an OOM error — it was caused by the
//  -mfix-esp32-psram-cache-issue build flag (an ESP32 original fix that
//  actively breaks ESP32-S3 PSRAM at boot). That flag has been removed.
//  These PSRAM overrides are still good practice to keep headroom for
//  larger models in the future.
// =============================================================================

#include "ei_psram_alloc.h"
#include "esp_heap_caps.h"
#include <cstdlib>
#include <cstdio>

// -----------------------------------------------------------------------------
//  ei_malloc — PSRAM-preferred malloc
//  Strong symbol that overrides __attribute__((weak)) in ei_classifier_porting.cpp
//
//  NOTE on alignment: The original porting code uses heap_caps_aligned_alloc(16,...)
//  for ESP32-S3 due to ESP-NN alignment requirements (github.com/espressif/esp-nn/issues/7).
//  Since we disabled ESP-NN with -DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=0 the 16-byte
//  alignment requirement is lifted and standard heap_caps_malloc is safe to use.
//  If you ever re-enable ESP-NN, switch to heap_caps_aligned_alloc below.
// -----------------------------------------------------------------------------
extern "C" void* ei_malloc(size_t size) {
    // Try PSRAM first
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr != nullptr) {
        return ptr;
    }
    // PSRAM full or not available — fall back to internal RAM
    ptr = malloc(size);
    if (ptr == nullptr) {
        // Both failed — log so we can diagnose
        printf("[EI] ei_malloc(%u) FAILED — both PSRAM and internal RAM exhausted!\n",
               (unsigned)size);
    }
    return ptr;
}

// -----------------------------------------------------------------------------
//  ei_calloc — zero-initialized PSRAM-preferred calloc
// -----------------------------------------------------------------------------
extern "C" void* ei_calloc(size_t nitems, size_t size) {
    void* ptr = heap_caps_calloc(nitems, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr != nullptr) {
        return ptr;
    }
    ptr = calloc(nitems, size);
    if (ptr == nullptr) {
        printf("[EI] ei_calloc(%u, %u) FAILED — both PSRAM and internal RAM exhausted!\n",
               (unsigned)nitems, (unsigned)size);
    }
    return ptr;
}

// -----------------------------------------------------------------------------
//  ei_free — works correctly for both PSRAM and internal RAM pointers
//  heap_caps_free() == free() on ESP-IDF; both handle any heap region.
// -----------------------------------------------------------------------------
extern "C" void ei_free(void* ptr) {
    free(ptr);
}
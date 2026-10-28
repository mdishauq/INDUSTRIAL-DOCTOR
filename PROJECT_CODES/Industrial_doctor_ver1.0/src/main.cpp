// =============================================================================
//  Industrial Doctor v2.1 — ESP32-S3 N16R8
//  Edge Impulse Audio Classifier
//
//  Key findings from library analysis:
//  - EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE = 14214 (~14KB) — fits in internal RAM
//  - EI_CLASSIFIER_LABEL_COUNT = 3 — three labels, not two
//  - Boot crash was -mfix-esp32-psram-cache-issue flag (removed in platformio.ini)
//  - ei_malloc overrides now use strong symbols via ei_psram_alloc.cpp
//
//  FILE LAYOUT:
//    src/main.cpp                                          <- this file
//    lib/industrial_doctor_inferencing/src/ei_psram_alloc.h
//    lib/industrial_doctor_inferencing/src/ei_psram_alloc.cpp
// =============================================================================

// CRITICAL INCLUDE ORDER — ei_psram_alloc.h must come before the EI header
// so the strong symbols are declared before Edge Impulse headers see them.
#include "ei_psram_alloc.h"
#include <industrial_doctor_inferencing.h>

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>
#include "esp_task_wdt.h"

// =============================================================================
//  Configuration
// =============================================================================

// --- WiFi --- credentials are in src/secrets.h (git-ignored — never commit)
// Copy src/secrets.h.example → src/secrets.h and fill in your values.
#include "secrets.h"

// --- I2S Mic Pins (INMP441) ---
#define I2S_WS   13   // Word Select / LRCK
#define I2S_SD   11   // Serial Data
#define I2S_SCK  12   // Bit Clock
#define I2S_PORT I2S_NUM_0

// --- Classification thresholds ---
#define THRESHOLD_CONFIDENT  0.70f   // Minimum confidence to make a call

// --- Upload rate limiting ---
#define UPLOAD_EVERY_N      3        // Upload every N inferences to reduce WiFi overhead

// =============================================================================
//  Global audio buffers — allocated in PSRAM in setup()
//
//  Two separate buffers are required:
//    sample_buffer : raw I2S int16 samples   (16000 × 2 bytes = 32 KB)
//    float_buffer  : converted float samples (16000 × 4 bytes = 64 KB)
//
//  The old code cast int16* → float* in-place. This is WRONG:
//    int16 = 2 bytes/sample, float = 4 bytes/sample.
//    In-place conversion overwrites unread int16 samples and overflows the
//    buffer by 2x causing silent heap corruption.
// =============================================================================
static int16_t* sample_buffer = nullptr;
static float*   float_buffer  = nullptr;

// =============================================================================
//  I2S initialization
// =============================================================================
static bool init_i2s() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = (uint32_t)EI_CLASSIFIER_FREQUENCY,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 64,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_SCK,
        .ws_io_num    = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD
    };

    if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) return false;
    if (i2s_set_pin(I2S_PORT, &pins)                != ESP_OK) return false;
    i2s_zero_dma_buffer(I2S_PORT);
    return true;
}

// =============================================================================
//  WiFi (non-blocking with timeout)
// =============================================================================
static void connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("  Connecting");
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
        esp_task_wdt_reset();   // Feed WDT — WiFi connect can take > 5 s
        delay(500); Serial.print("."); yield();
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n  ✓ Connected — IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n  ⚠ Timeout — offline mode");
    }
}

// =============================================================================
//  HTTP upload
// =============================================================================
static void upload_result(const String& status, float scores[], int n_labels, float rms_db) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(SERVER_URL);
    http.setTimeout(5000);   // 5 s max — default TCP timeout is ~10 s which triggers WDT
    http.addHeader("Content-Type", "application/json");

    // Build a JSON payload with all label scores
    String payload = "{\"status\":\"" + status + "\",\"rms_db\":" + String(rms_db, 1) + ",\"scores\":{";
    for (int i = 0; i < n_labels; i++) {
        payload += "\"";
        payload += String(ei_classifier_inferencing_categories[i]);
        payload += "\":";
        payload += String(scores[i], 4);
        if (i < n_labels - 1) payload += ",";
    }
    payload += "}}";

    esp_task_wdt_reset();    // feed before POST — TCP connect can block
    int code = http.POST(payload);
    esp_task_wdt_reset();    // feed after POST
    Serial.printf("  Upload %s (HTTP %d)\n", code > 0 ? "OK" : "FAILED", code);
    http.end();
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
    // =========================================================================
    // STEP 0: Extend and subscribe the Task WDT BEFORE any other code runs.
    //
    // ESP-IDF 5.x default WDT timeout = 5 s. Framework init (OPI PSRAM
    // calibration, USB CDC enum, loopTask stack alloc) already consumes ~1-2 s
    // BEFORE setup() is called. A single blocking call (psramInit, WiFi, I2S)
    // longer than the remaining window causes TG0WDT_SYS_RST.
    //
    // Extending to 30 s gives every init step a safe margin.
    // esp_task_wdt_add(NULL) subscribes the loopTask; if the Arduino framework
    // already subscribed it, the call returns ESP_ERR_INVALID_ARG — that is
    // harmless and intentionally not checked here.
    //
    // ⚠ API DIFFERENCE:
    //   ESP-IDF 4.x (Arduino-ESP32 2.x): esp_task_wdt_init(uint32_t, bool)
    //   ESP-IDF 5.x (Arduino-ESP32 3.x): esp_task_wdt_init(const esp_task_wdt_config_t*)
    // The #if below compiles correctly on both toolchains.
    // =========================================================================
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    {   // IDF 5.x — config-struct API
        esp_task_wdt_config_t wdt_cfg = {
            .timeout_ms    = 30000,   // 30 s
            .idle_core_mask = 0,
            .trigger_panic  = true
        };
        esp_task_wdt_reconfigure(&wdt_cfg);  // reconfigure the pre-existing TWDT
    }
#else
    // IDF 4.x — simple timeout + panic args
    esp_task_wdt_init(30, true);
#endif
    esp_task_wdt_add(NULL);   // subscribe loopTask (safe to call twice)
    esp_task_wdt_reset();     // reset the counter immediately

    Serial.begin(115200);

    // Wait up to 3 s for USB CDC to enumerate — but yield() on every iteration
    // so the Task WDT is fed. A hard delay(3000) blocks without feeding the WDT
    // and was the direct trigger for the TG0WDT_SYS_RST boot loop.
    for (uint32_t t = millis(); !Serial && (millis() - t) < 3000; ) {
        esp_task_wdt_reset();
        delay(10);
    }
    esp_task_wdt_reset();

    Serial.println(F("\n╔════════════════════════════════════════╗"));
    Serial.println(F(  "║  Industrial Doctor v2.1 — ESP32-S3 N16R8  ║"));
    Serial.println(F(  "╚════════════════════════════════════════╝\n"));

    // -------------------------------------------------------------------------
    //  1. PSRAM
    // -------------------------------------------------------------------------
    Serial.println(F("[1/5] PSRAM..."));
    esp_task_wdt_reset();   // psramInit() runs OPI calibration — can take >1 s
    if (!psramInit()) {
        Serial.println(F("╔══════════════════════════════════════════════╗"));
        Serial.println(F("║  FATAL: PSRAM INIT FAILED                    ║"));
        Serial.println(F("║  Check platformio.ini:                       ║"));
        Serial.println(F("║    board_build.arduino.memory_type = qio_opi ║"));
        Serial.println(F("║    -DBOARD_HAS_PSRAM                         ║"));
        Serial.println(F("║    -DCONFIG_SPIRAM_MODE_OCT=1                ║"));
        Serial.println(F("║  Do NOT use -mfix-esp32-psram-cache-issue    ║"));
        Serial.println(F("╚══════════════════════════════════════════════╝"));
        while (true) { esp_task_wdt_reset(); delay(2000); }  // WDT-fed halt
    }
    ei_psram_report();
    esp_task_wdt_reset();

    // -------------------------------------------------------------------------
    //  2. Audio buffers in PSRAM
    //  Total needed: 16000 × (2 + 4) = 96 KB — well within 8 MB PSRAM
    // -------------------------------------------------------------------------
    Serial.println(F("[2/5] Audio buffers..."));
    const size_t N = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;

    sample_buffer = (int16_t*)heap_caps_malloc(N * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    float_buffer  = (float*)  heap_caps_malloc(N * sizeof(float),   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!sample_buffer) { Serial.println(F("  sample_buffer PSRAM miss → internal")); sample_buffer = (int16_t*)malloc(N * sizeof(int16_t)); }
    if (!float_buffer)  { Serial.println(F("  float_buffer  PSRAM miss → internal")); float_buffer  = (float*)  malloc(N * sizeof(float));   }

    if (!sample_buffer || !float_buffer) {
        Serial.printf("FATAL: Buffer alloc failed. Need %u bytes.\n", (unsigned)(N * 6));
        while (true) { esp_task_wdt_reset(); delay(1000); }
    }
    Serial.printf("  ✓ sample_buffer @ 0x%08X  (%u bytes)\n", (unsigned)sample_buffer, (unsigned)(N*2));
    Serial.printf("  ✓ float_buffer  @ 0x%08X  (%u bytes)\n", (unsigned)float_buffer,  (unsigned)(N*4));
    Serial.printf("  PSRAM remaining: %u bytes\n", (unsigned)ESP.getFreePsram());
    esp_task_wdt_reset();

    // -------------------------------------------------------------------------
    //  3. WiFi
    // -------------------------------------------------------------------------
    Serial.println(F("[3/5] WiFi..."));
    connect_wifi();
    esp_task_wdt_reset();

    // -------------------------------------------------------------------------
    //  4. I2S
    // -------------------------------------------------------------------------
    Serial.println(F("[4/5] I2S microphone..."));
    esp_task_wdt_reset();   // I2S DMA buffer init can stall briefly
    if (!init_i2s()) {
        Serial.println(F("FATAL: I2S init failed. Check wiring: WS=13  SD=11  SCK=12"));
        while (true) { esp_task_wdt_reset(); delay(1000); }
    }
    Serial.println(F("  ✓ I2S ready"));
    esp_task_wdt_reset();

    // -------------------------------------------------------------------------
    //  5. Print EI model info
    //  Your model has 3 labels — print all of them so you can verify the names
    // -------------------------------------------------------------------------
    Serial.println(F("[5/5] Model info:"));
    Serial.printf("  Sample rate  : %d Hz\n",    EI_CLASSIFIER_FREQUENCY);
    Serial.printf("  Window size  : %d samples (%.2f s)\n",
                  EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,
                  (float)EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / EI_CLASSIFIER_FREQUENCY);
    Serial.printf("  Arena size   : %d bytes (~%d KB) — fits in internal RAM\n",
                  EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE,
                  EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE / 1024);
    Serial.printf("  Label count  : %d\n", EI_CLASSIFIER_LABEL_COUNT);
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.printf("    [%d] \"%s\"\n", i, ei_classifier_inferencing_categories[i]);
    }

    Serial.println(F("\n✓ Boot complete. Starting inference...\n"));
    esp_task_wdt_reset();   // Feed once more before handing off to loop()
}

// =============================================================================
//  LOOP
// =============================================================================
void loop() {
    static uint32_t inference_count = 0;
    uint32_t t_start = millis();

    // -------------------------------------------------------------------------
    //  1. Collect audio
    //  INMP441 outputs 24-bit MSB-aligned in 32-bit frames.
    //  Shift right 14 gives a well-scaled int16 value.
    // -------------------------------------------------------------------------
    const size_t N = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    float sum_sq = 0.0f;

    for (size_t i = 0; i < N; i++) {
        int32_t raw = 0;
        size_t  bytes_read = 0;
        // portMAX_DELAY would block forever on a DMA stall → WDT reset.
        // 10 ms is ample for a 64-frame DMA buffer at 16 kHz (~4 ms fill time).
        i2s_read(I2S_PORT, &raw, sizeof(raw), &bytes_read, pdMS_TO_TICKS(10));

        sample_buffer[i] = (int16_t)(raw >> 14);
        float norm = sample_buffer[i] / 32768.0f;
        sum_sq += norm * norm;

        if ((i & 0xFF) == 0) esp_task_wdt_reset();  // yield() only feeds FreeRTOS, not Task WDT
    }
    uint32_t t_collect = millis() - t_start;

    float rms    = sqrtf(sum_sq / N);
    float rms_db = (rms > 1e-9f) ? 20.0f * log10f(rms) : -90.0f;

    // -------------------------------------------------------------------------
    //  2. Convert int16 → float using SEPARATE buffers
    //  DO NOT cast sample_buffer to float* — that corrupts memory.
    // -------------------------------------------------------------------------
    numpy::int16_to_float(sample_buffer, float_buffer, N);

    // -------------------------------------------------------------------------
    //  3. Run inference
    // -------------------------------------------------------------------------
    signal_t signal;
    if (numpy::signal_from_buffer(float_buffer, N, &signal) != 0) {
        Serial.println(F("✗ signal_from_buffer failed"));
        delay(500); return;
    }

    ei_impulse_result_t result = { 0 };
    esp_task_wdt_reset();   // TFLite arena init on first call can take seconds
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
    uint32_t t_total = millis() - t_start;

    if (err != EI_IMPULSE_OK) {
        Serial.printf("✗ run_classifier error: %d\n", err);
        delay(500); return;
    }

    // -------------------------------------------------------------------------
    //  4. Parse results
    //  Collect all scores, find the winning label generically.
    //  This works correctly whether you have 2 or 3 labels.
    //  The serial output will show you all label names on first boot —
    //  update the status logic below once you know your 3rd label name.
    // -------------------------------------------------------------------------
    float scores[EI_CLASSIFIER_LABEL_COUNT];
    int   best_idx   = 0;
    float best_score = 0.0f;

    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        scores[i] = result.classification[i].value;
        if (scores[i] > best_score) {
            best_score = scores[i];
            best_idx   = i;
        }
    }

    // Map best label → human-readable status
    // Update these string comparisons once you confirm your 3 label names from serial output
    String best_label = String(ei_classifier_inferencing_categories[best_idx]);
    String status;

    if (best_score < THRESHOLD_CONFIDENT) {
        status = "Uncertain";                       // No label met the confidence threshold
    } else if (best_label == "normal") {
        status = "Healthy";
    } else if (best_label == "abnormal") {
        status = "Critical";
    } else {
        // Third label — e.g. "noise", "idle", "unknown"
        // Change "Noise/Other" to match once you see the label name in serial output
        status = "Noise/Other (" + best_label + ")";
    }

    inference_count++;

    // -------------------------------------------------------------------------
    //  5. Print results
    // -------------------------------------------------------------------------
    Serial.println(F("──────────────────────────────────────────────"));
    Serial.printf("  #%u | Status: %s\n", inference_count, status.c_str());
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.printf("    %-12s : %.4f%s\n",
                      ei_classifier_inferencing_categories[i],
                      scores[i],
                      (i == best_idx) ? " ← best" : "");
    }
    Serial.printf("  RMS: %.1f dBFS | collect: %ums | total: %ums\n",
                  rms_db, t_collect, t_total);
    Serial.printf("  Free PSRAM: %u bytes | Free heap: %u bytes\n",
                  (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());

#if EI_CLASSIFIER_HAS_ANOMALY == 1
    Serial.printf("  Anomaly score: %.4f\n", result.anomaly);
#endif

    // -------------------------------------------------------------------------
    //  6. Upload (rate limited)
    // -------------------------------------------------------------------------
    if (inference_count % UPLOAD_EVERY_N == 0) {
        upload_result(status, scores, EI_CLASSIFIER_LABEL_COUNT, rms_db);
    }

    delay(200);
    yield();
}
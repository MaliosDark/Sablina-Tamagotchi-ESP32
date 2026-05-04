#include "internal_llm_bridge.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "esp_log.h"
#include "esp_system.h"

#include "llm.h"

static const char *TAG = "INT_LLM_BRIDGE";

static Transformer g_transformer;
static Tokenizer g_tokenizer;
static Sampler g_sampler;

static bool g_initialized = false;
static bool g_ready = false;

static char g_token_buf[384];
static size_t g_token_len = 0;

static const char *k_checkpoint_candidates[] = {
    "/spiffs/data/stories260K.bin",
    "/spiffs/stories260K.bin",
    "/data/stories260K.bin",
    "/stories260K.bin",
};

static const char *k_tokenizer_candidates[] = {
    "/spiffs/data/tok512.bin",
    "/spiffs/tok512.bin",
    "/data/tok512.bin",
    "/tok512.bin",
};

static bool file_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0;
}

static const char *first_existing(const char *const *candidates, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        if (file_exists(candidates[i])) {
            return candidates[i];
        }
    }
    return NULL;
}

static void token_cb(const char *piece)
{
    size_t i;
    if (!piece || !piece[0]) {
        return;
    }

    for (i = 0; piece[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)piece[i];
        if ((isprint(c) || isspace(c)) && g_token_len < sizeof(g_token_buf) - 1) {
            g_token_buf[g_token_len++] = (char)c;
        }
    }
    g_token_buf[g_token_len] = '\0';
}

static void done_cb(float tokens_ps)
{
    ESP_LOGI(TAG, "generate done %.2f tok/s", tokens_ps);
}

bool internal_llm_begin(void)
{
    const char *checkpoint;
    const char *tokenizer;

    if (g_initialized) {
        return g_ready;
    }
    g_initialized = true;

    checkpoint = first_existing(k_checkpoint_candidates,
                                sizeof(k_checkpoint_candidates) / sizeof(k_checkpoint_candidates[0]));
    tokenizer = first_existing(k_tokenizer_candidates,
                               sizeof(k_tokenizer_candidates) / sizeof(k_tokenizer_candidates[0]));

    if (!checkpoint || !tokenizer) {
        ESP_LOGW(TAG, "internal assets not found (checkpoint/tokenizer)");
        return false;
    }

    ESP_LOGI(TAG, "loading checkpoint=%s tokenizer=%s", checkpoint, tokenizer);
    build_transformer(&g_transformer, (char *)checkpoint);
    build_tokenizer(&g_tokenizer, (char *)tokenizer, g_transformer.config.vocab_size);
    build_sampler(&g_sampler, g_transformer.config.vocab_size, 1.0f, 0.9f,
                  (unsigned long long)time(NULL));

    g_ready = true;
    ESP_LOGI(TAG, "internal llm ready vocab=%d seq=%d",
             g_transformer.config.vocab_size, g_transformer.config.seq_len);
    return true;
}

bool internal_llm_is_ready(void)
{
    return g_ready;
}

bool internal_llm_generate(const char *prompt, int steps, char *out, size_t out_len)
{
    size_t end;

    if (!g_ready || !prompt || !out || out_len == 0) {
        return false;
    }

    if (steps <= 0) {
        steps = 24;
    }

    g_token_len = 0;
    g_token_buf[0] = '\0';

    generate_with_callbacks(&g_transformer,
                            &g_tokenizer,
                            &g_sampler,
                            (char *)prompt,
                            steps,
                            done_cb,
                            token_cb);

    end = g_token_len;
    while (end > 0 && isspace((unsigned char)g_token_buf[end - 1])) {
        end--;
    }
    g_token_buf[end] = '\0';

    if (g_token_buf[0] == '\0') {
        return false;
    }

    strlcpy(out, g_token_buf, out_len);
    return true;
}

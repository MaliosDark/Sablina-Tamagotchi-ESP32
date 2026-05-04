#ifndef INTERNAL_LLM_BRIDGE_H
#define INTERNAL_LLM_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool internal_llm_begin(void);
bool internal_llm_is_ready(void);
bool internal_llm_generate(const char *prompt, int steps, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif

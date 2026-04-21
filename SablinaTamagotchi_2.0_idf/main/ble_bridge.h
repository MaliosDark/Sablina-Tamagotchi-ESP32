#pragma once

#include <stdbool.h>
#include <stddef.h>

void ble_bridge_init(void);
void ble_bridge_notify_text(const char *text);
bool ble_bridge_poll_text(char *from_name, size_t from_name_len, char *text, size_t text_len);
bool ble_bridge_peer_visible(void);
const char *ble_bridge_peer_name(void);

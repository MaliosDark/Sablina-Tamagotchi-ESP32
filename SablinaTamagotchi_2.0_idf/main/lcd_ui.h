#pragma once
#include <stdint.h>

/* Initialise ST7789T with exact Arduino pin config (BL=46, MOSI=45, SCLK=40, CS=42, DC=41, RST=39).
   Must be called from app_main before the agent task starts. */
void lcd_ui_init(void);

/* Render status bar + LLM thought + tool result to the display.
   Called after each ReAct cycle. */
void lcd_ui_update(int hunger, int rest, int clean, int coins,
                   const char *tool, const char *thought, const char *result);

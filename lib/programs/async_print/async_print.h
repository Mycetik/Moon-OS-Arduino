#ifndef ASYNC_PRINT_H
#define ASYNC_PRINT_H

#include <stdint.h>
#include <Arduino.h>

void asyncPrint_init(void);

void async_print(const char* str);

// Фукнція щоб читати рядки прямо з flash пам'яті, замість ОЗП
void async_print_P(const __FlashStringHelper* str);

#endif
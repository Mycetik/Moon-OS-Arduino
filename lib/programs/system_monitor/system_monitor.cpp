#include "system_monitor.h"
#include <MoonOS.h>
#include "programs/async_print/async_print.h"
#include <stdio.h>
#include <avr/pgmspace.h>

/*
system_monitor.h - це програма яка друкує стан системи, скільки потоків, скільки зайнято оперативнлї пам'яті і так далі.
Для роботи цієї програми необхідно спочатку запустити async_print.h
*/

CREATE_STACK(monitorStack, 70);
static uint16_t monitorInterval = 2000;

void taskSystemMonitor() {
    char lineBuf[40];

    while(1) {
        thread_delay(monitorInterval);

        // Друкуємо текст окремими рядками
        
        // Використовуємо snprintf_P та PSTR(), для економії ОЗП.
        // Системний монітор читає текст який написаний всередині PSTR прямо з flash пам'яті, а не ОЗП, це економія пам'яті.
        // Якщо підсумувати, то читаємо шаблон тексту з диска, підставляємо замість %d наші цифри і все, готово
        snprintf_P(lineBuf, sizeof(lineBuf), PSTR("\r\nSYSTEM MONITOR\r\nRAM: %d / 2048\r\n"), 2048 - getFreeRam());
        async_print(lineBuf);
        thread_delay(5); 
        
        for(uint8_t i = 0; i < getThreadCount(); i++) {
            // Тут також читаємо усе з диска через snprintf_P та PSTR(). Економимо ОЗП ще раз.
            snprintf_P(lineBuf, sizeof(lineBuf), PSTR("ID[%d]: %d / %d bytes\r\n"),
                getThreadIdByIndex(i),
                getThreadTotalStack(i) - getThreadFreeStack(i),
                getThreadTotalStack(i));
            async_print(lineBuf);
            thread_delay(5);
        }
    }
}

void sysmon_enable(uint16_t updateIntervalMs) {
    monitorInterval = updateIntervalMs;
    createThread(taskSystemMonitor, 3, monitorStack, sizeof(monitorStack), 255);
}
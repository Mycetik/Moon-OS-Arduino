#include "core.h"
#include <avr/interrupt.h>
#include <avr/io.h>

static void (*user_panic_hook)(void) = NULL;

// Функція, що дозволяє виконати певні дії під час паніки ядра.
void setPanicHook(void (*hookFunc)(void)) {
    user_panic_hook = hookFunc;
}

// Низькорівнева функція друку, яка необхідна через те, що звичайні функції не працюють в режимі паніки.
void panic_print(const char* str) {
    while (*str) {
        while (!(UCSR0A & (1 << UDRE0))); 
        UDR0 = *str++;
    }
}

// Функція яка викликає Kernel Panic. Просто функція яка викликається при критичних помилках системи.
void panic(const char* reason) {
    cli(); // Зупиняємо усі задачі.

    // Викликаємо код користувача системи, який він повинен написати на такий випадок.
	if (user_panic_hook != NULL) {
        user_panic_hook();
    }

    // Налаштовуємо 13-тий пін як output (регістр B, 5-тий біт). Це вбудований в Arduino світлодіод.
    DDRB |= (1 << 5);

    panic_print("\r\n    X_X\r\n");
    panic_print("\r\nKERNEL PANIC:\r\n");
    panic_print(reason);
    panic_print("\r\n");

    // Блимаємо світлодіодом напряму через регістри процесора та зависання на 100мс.
    while(1) {
        PORTB |= (1 << 5); // Увімкнути
        _delay_ms(100);
        PORTB &= ~(1 << 5); // Вимкнути
        _delay_ms(100);
    }
}
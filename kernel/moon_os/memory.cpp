#include <core.h>

extern int __bss_end;
extern void *__brkval;

int getFreeRam(void) {
if ((int)__brkval == 0) {
        return (RAMEND - (int)&__bss_end);
    }
    else {
        return (RAMEND - (int)__brkval);
    }
}

// Повертає кількість вільних байтів у стеку потоку.
int getThreadFreeStack(uint8_t taskIndex) {
    // Увесь вільний простір в пам'яті потоку зайнятий спеціальним символом.
    // Тому щоб порахувати цей вільний простір необхідно порахувати кількість цих символів.

    // Перевіряємо чи існує взагалі цей потік.
    if (taskIndex >= taskCount) return -1;
    
    int freeBytes = 0;
    uint8_t* ptr = tasks[taskIndex].stackBottom;
    
    // Рахуємо з дна стека вгору.
    while (*ptr == STACK_CANARY) {
        freeBytes++;
        ptr++;
    }
    
    return freeBytes;
}

// Повертає загальний розмір стеку потоку
int getThreadTotalStack(uint8_t taskIndex) {
    // Перевіряємо чи існує взагалі цей потік.
    if (taskIndex >= taskCount) return -1;
    // Просто отримуємо значення з потоку.
    return tasks[taskIndex].stackSize;
}
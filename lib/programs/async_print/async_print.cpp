#include "async_print.h"
#include <MoonOS.h>

/*
async_print.h - це програма, яка додає в систему потокобезпечний print.
Він створює чергу для повідомлень та фоновий потік, який читає чергу та виводить повідолмення.
*/

#define PRINT_QUEUE_CAPACITY 12 // Максимальна кількість повідомлень у черзі
#define FLASH_FLAG 0x8000 // Маркер того, що дані взяті з флеш пам'яті

static void* printQueueBuffer[PRINT_QUEUE_CAPACITY];
static Queue printQueue;
CREATE_STACK(printDaemonStack, 20);

void taskPrintDaemon(void) {
    while(1) {
        void* msg = queueReceive(&printQueue);

        // Перетворюємо вказівник у 16 бітне число
        uint16_t addr = (uint16_t)msg; 
        
        // Заборонємо усім потокам окрім цього використовувати print
        serialLock();
        
        // В Arduino вказівник на пам'ять займає 2 байти(16 бітів).
        // І в ОЗП і в Flash пам'яті останній біт адреси завжди дорівнює нулю.
        // Тому тут ми використовуємо цей останній біт як марку, щоб відрізнити дані з флеш пам'яті від даних з ОЗП.
        // Фукція async_print_P додає в той самий пустий біт марку, яка означає що це дані з флеш пам'яті.

        // Перевіряємо марку
        if (addr & FLASH_FLAG) {
        	// Якщо дані з флеш пам'яті, то:
            // Скидаємо мітку
            addr &= ~FLASH_FLAG; 
            // Кажемо ардуїні, що це дані з флеш пам'яті, щоб вони читала їх не з ОЗП
            Serial.print((const __FlashStringHelper*)addr); 
        } else {
            // Якщо дані з флеш пам'яті, то просто читаємо їх з ОЗП
            Serial.print((const char*)addr);
        }
        
        // Дозволяємо використовувати print знову
        serialUnlock();
    }
}

void asyncPrint_init(void) {
	// Створюємо чергу(МАЙ ІНГЛІШ ЛЕВЕЛ ІС КУВЕУВЕ)
    queueInit(&printQueue, printQueueBuffer, PRINT_QUEUE_CAPACITY);
    // Запускаємо фоновий потік, який буде обробляти чергу
    createThread(taskPrintDaemon, 3, printDaemonStack, sizeof(printDaemonStack), 254);
}

void async_print(const char* str) {
	// Просто кладемо вказівник на рядок у чергу
    queueSend(&printQueue, (void*)str);
}

void async_print_P(const __FlashStringHelper* str) {
    uint16_t addr = (uint16_t)str;
    addr |= FLASH_FLAG; // Встановлюємо то самий маркер
    queueSend(&printQueue, (void*)addr);
}
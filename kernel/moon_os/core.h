#ifndef CORE_H
#define CORE_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_CONTEXT_PEAK 53

#define OS_SAFETY_MARGIN 40

#define OS_SYSTEM_RESERVE (OS_CONTEXT_PEAK + OS_SAFETY_MARGIN)

// Макрос для створення стека
#define CREATE_STACK(name, usable_size) uint8_t name[(usable_size) + OS_SYSTEM_RESERVE]

#define MAX_TASKS 10 // Максимальна кількість потоків.
#define STACK_CANARY 0xAA // Байт для перевірки стека.

// Структура потоку
typedef struct {
    uint8_t* sp; // Вказівник на верхівку стека
    uint8_t* stackBottom; // Вказує на найнижчу адресу виділеної пам'яті(стека), де лежить спеціальний символ
    uint16_t stackSize; // Розмір стека
    uint8_t priority; // Пріорітет потоку
    uint8_t timeSlice; // Ліміт часу(квант)
    uint8_t ticksLeft; // Таймер роботи потоку. Тобто скільки мілісекунд залишилось до кінця кванту
    volatile bool ready; // Готовність потока працювати
    volatile bool suspended; // Заморозка потока для функції зупинки потоку
    volatile uint64_t wakeUpTime; // Зберігає абсолютний системний час, коли потік має прокинутися.
    uint8_t id; // Універсальний id потоку
} TCB; // Task Control Block

extern TCB tasks[MAX_TASKS];
extern volatile uint8_t taskCount;
extern volatile uint8_t currentTask;
extern volatile uint64_t sys_millis;


// Базові функції
void os_init(void);
void os_start(void);
bool createThread(void (*taskFunc)(void), uint8_t priority, uint8_t* stack, uint16_t stackSize, uint8_t id);
void suspendThread(uint8_t id);
void resumeThread(uint8_t id);
void thread_delay(uint32_t ms);
uint64_t thread_millis(void);


// М'ютекси
typedef struct {
    volatile bool isLocked;
    volatile uint8_t ownerTask;
} Mutex;
void mutexLock(Mutex* m);
void mutexUnlock(Mutex* m);
void serialLock(void);
void serialUnlock(void);


// KERNEL PANIC
void panic(const char* reason);
void setPanicHook(void (*hookFunc)(void));


// Геттери, сеттери
uint8_t getThreadCount(void);
uint8_t getCurrentPriority(void);
int findThreadById(uint8_t id);
uint8_t getThreadIdByIndex(uint8_t index);


// Оперативна пам'ять
int getThreadTotalStack(uint8_t taskIndex);
int getThreadFreeStack(uint8_t taskIndex);
int getFreeRam(void);


// Черга
typedef struct {
    void** buffer; // Вказівник на масив, де будуть зберігатися елементи
    uint8_t capacity; // Максимальна кількість елементів, тобто розмір масиву
    volatile uint8_t head; // Куди писати наступний елемент
    volatile uint8_t tail; // Звідки читати наступний елемент
    volatile uint8_t count; // Показує кількість елементів у черзі
} Queue;
void queueInit(Queue* q, void** buffer, uint8_t capacity);
void queueSend(Queue* q, void* msg);
void* queueReceive(Queue* q);


#ifdef __cplusplus
}
#endif

#endif
#include "core.h"
#include <avr/interrupt.h>
#include <avr/io.h>

// Цей файл додає в ОС API для створення черги

void queueInit(Queue* q, void** buffer, uint8_t capacity) {
    cli(); // Забороняємо переривання процесора під час налаштування
    q->buffer = buffer;
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    sei(); // Дозволяємо працювати назад
}

void queueSend(Queue* q, void* msg) {
    while(1) {
		uint8_t oldSREG = SREG; // Забороняємо переривання процесора
        cli();
        
        // Перевіряємо, чи є вільне місце в черзі
        if (q->count < q->capacity) {
            // Записуємо повідомлення туди, куди вказує head
            q->buffer[q->head] = msg;
            
            // Зсуваємо head вперед
            q->head = (q->head + 1) % q->capacity;
            
            q->count++; // Збільшуємо лічильник елементів
            SREG = oldSREG;
            return;
        }
        
        SREG = oldSREG; // Продовжуємо переривання знову
        // Якщо черга повна, то віддаємо час іншим потокам (можливо хтось прочитає елементи з черги і звільнить місце)
        thread_delay(1);
    }
}

void* queueReceive(Queue* q) {
    while(1) {
        cli(); // Забороняємо переривання процесора
        
        // Перевірка чи взагалі є що читати
        if (q->count > 0) {
            // Беремо повідомлення з того місця куди вказує tail
            void* msg = q->buffer[q->tail];
            
            // Зсуваємо tail вперед
            q->tail = (q->tail + 1) % q->capacity;
            
            q->count--; // Зменшуємо лічильник елементів
            sei();
            return msg; // Повертаємо повідомлення
        }
        
        sei();
        // Якщо черга порожня, то просто засинаємо
        thread_delay(1);
    }
}

// Спеціальна функція для виклику ТІЛЬКИ з ISR(для виклику з апаратних переривань)
bool queueSendFromISR(Queue* q, void* msg) {

    if (q->count < q->capacity) {
        q->buffer[q->head] = msg;
        q->head = (q->head + 1) % q->capacity;
        q->count++;
        return true;
    }
    
    // Якщо черга повна, ми просто викидаємо подію.
    return false; 
}
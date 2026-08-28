#include "core.h"
#include <avr/interrupt.h>
#include <avr/io.h>

// Цей файл додає в систему м'ютекси

// Основні функції м'ютексів
void mutexLock(Mutex* m) {
    while(1) {
        cli();
        if(!m->isLocked) {
            m->isLocked = true;
            m->ownerTask = currentTask;
            sei();
            return;
        }
        sei();
        thread_delay(1);
    }
}

void mutexUnlock(Mutex* m) {
    cli();
    if(m->isLocked && m->ownerTask == currentTask) {
        m->isLocked = false;
    }
    sei();
}


// Функції для блоування print якщо його вже хтось використовує
Mutex sys_serialMutex = {false, 0};

void serialLock() {
	mutexLock(&sys_serialMutex);
}

void serialUnlock() {
	mutexUnlock(&sys_serialMutex);
}
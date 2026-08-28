#include <core.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

/*
Це планувальник завдань системи, написаний спеціально під Arduino UNO R3.
Він додає головний функціонал системи, а точніше дозволяє створювати потоки, взаємодіяти з ними та розподіляє процесорний час.

Цей планувальник заснований на витісняльній багатозадачності із квантуванням часу. А якщо казати по простому то він працює так:
Припустимо, що розробник створює 3 потоки:
Потік1
Потік2
Потік3
Пріорітет кожного потоку відповідає його номеру.
Система бачить що є потік 1 з пріорітетом 1 і починає першим його виконувати, бо у нього найкращий пріорітет з усіх.
Далі виконуються інші потоки.
Система дозволяє створити потік з пріорітетом від 0 до 3, чим нижче значення тим потік приорітетніше.

У кожного потоку є свій ліміт часу, скільки він може безперервно займати процесор(квант).
Наприклад, якщо потік робить якусь довгу та складну операцію, займаючи процесор і не даючи його іншим потокам, то
система через певний проміжок часу(один квант часу) забере у нього процесор примусово і поставить на виконання наступний потік.
Квант залежить від пріорітету:
Пріорітет1 - 15мс
Пріорітет2 - 10мс
Пріорітет3 - 5мс
А якщо створити потік з пріорітетом 0, то у нього взагалі не буде кванту часу і він зможе взяти процесор повністю.
Тобто якщо створити потік з пріорітетом 0 то процесор завжди буде виконувати його в першу чергу і давати йому не обежений час.

Таким чином система гарно працює навіть якщо декілька потоків одразу виконують складні та довгі операції.

Через фізичну простоту Arduino в системі використовується статичне виділення пам'яті,
тобто для кожного процесу пам'ять виділяється ще на етапі компіляції.
Це викликає деякі проблеми, наприклад по справжньому вбити потік в системі не можливо, бо ми не можемо просто забрати його
пам'ять назад, вона назавжди зарезервована компілятором. Система може лише не давати потоку процесорний час.
А ще вихід одного потока за межі своєї пам'яті(стека) одразу викликає паніку всієї системи.
Також система споживає близько 800байт оперативної пам'яті, коли в arduino її всього 2кб.
Та і взагалі в системі багато таких обмежень через простоту Arduino.

Але не дивлячісь на фізичні обмеження система все одно стабільно працює і робить усе можливе щоб виконати свою задачу.
*/

// Макроси для збереження та відновлення стану процесора.
#define SAVE_CONTEXT() \
    asm volatile ( \
        "push r0 \n\t" "in r0, __SREG__ \n\t" "cli \n\t" "push r0 \n\t" \
        "push r1 \n\t" "clr r1 \n\t" \
        "push r2 \n\t" "push r3 \n\t" "push r4 \n\t" "push r5 \n\t" \
        "push r6 \n\t" "push r7 \n\t" "push r8 \n\t" "push r9 \n\t" \
        "push r10 \n\t" "push r11 \n\t" "push r12 \n\t" "push r13 \n\t" \
        "push r14 \n\t" "push r15 \n\t" "push r16 \n\t" "push r17 \n\t" \
        "push r18 \n\t" "push r19 \n\t" "push r20 \n\t" "push r21 \n\t" \
        "push r22 \n\t" "push r23 \n\t" "push r24 \n\t" "push r25 \n\t" \
        "push r26 \n\t" "push r27 \n\t" "push r28 \n\t" "push r29 \n\t" \
        "push r30 \n\t" "push r31 \n\t" \
    );

#define RESTORE_CONTEXT() \
    asm volatile ( \
        "pop r31 \n\t" "pop r30 \n\t" "pop r29 \n\t" "pop r28 \n\t" \
        "pop r27 \n\t" "pop r26 \n\t" "pop r25 \n\t" "pop r24 \n\t" \
        "pop r23 \n\t" "pop r22 \n\t" "pop r21 \n\t" "pop r20 \n\t" \
        "pop r19 \n\t" "pop r18 \n\t" "pop r17 \n\t" "pop r16 \n\t" \
        "pop r15 \n\t" "pop r14 \n\t" "pop r13 \n\t" "pop r12 \n\t" \
        "pop r11 \n\t" "pop r10 \n\t" "pop r9 \n\t" "pop r8 \n\t" \
        "pop r7 \n\t" "pop r6 \n\t" "pop r5 \n\t" "pop r4 \n\t" \
        "pop r3 \n\t" "pop r2 \n\t" "pop r1 \n\t" "pop r0 \n\t" \
        "out __SREG__, r0 \n\t" "pop r0 \n\t" \
    );

TCB tasks[MAX_TASKS];
volatile uint8_t taskCount = 0;
volatile uint8_t currentTask = 0;
volatile uint64_t sys_millis = 0;

extern "C" {
    volatile uint16_t current_sp_16 = 0;
}

extern "C" {
    uint8_t global_isr_stack[60]; 
}

// Продвинута функція millis.
uint64_t thread_millis(void) {
    uint64_t m;
    uint8_t oldSREG = SREG;
    cli();
    m = sys_millis;
    SREG = oldSREG;
    return m;
}

// Продвинута функція delay
void thread_delay(uint32_t ms) {
    cli();
    tasks[currentTask].ready = false;
    tasks[currentTask].wakeUpTime = sys_millis + ms;
    sei();
    while(!tasks[currentTask].ready) {
        asm volatile ("nop");
    }
}

// Ця фукнця викликається коли спрацьовує системний таймер номер 0 - тобто кожну мілісекунду.
extern "C" void system_tick(void) {
	// Визначаємо який зараз поток, точніше беремо вказівник на його стек.
    tasks[currentTask].sp = (uint8_t*)current_sp_16;

    // Стек використовується з кінця, тобто байт номер 0 - останній.
    // Якщо байт 0 НЕ дорівнює спеціальному символу STACK_CANARY, це означає що поток вийшов за межі своєї пам'яті.
    if (*(tasks[currentTask].stackBottom) != STACK_CANARY) {
        panic("Stack Overflow! (1)");
    }

    if (tasks[currentTask].stackBottom[OS_SAFETY_MARGIN - 1] != STACK_CANARY) {
        panic("Stack Overflow! (2)");
    }

    // Додаємо до кількості мілісекунд ще одну
    sys_millis++;

    // Перевіряємо кожен потік в масиві
    for(uint8_t i = 0; i < taskCount; i++) {
    	// Якщо час сплячки потока вийшов і його не призупинено, то встановлюємо значення ready на true, активуючи його
		if(!tasks[i].ready && sys_millis >= tasks[i].wakeUpTime && !tasks[i].suspended) {
            tasks[i].ready = true;
        }
    }

    // Перевіряємо, чи не нульовий це потік(потік диктатор з приорітетом 0)
    if (tasks[currentTask].priority > 0 && tasks[currentTask].ticksLeft > 0) {
    	// Якщо це не нульовний потік то віднімаємо від його ліміту 1 мілісекунду
    	// Тому жоден звичайний потік не зомже захопити процесор назавжди
        tasks[currentTask].ticksLeft--;
    }

    
    uint8_t nextTask = currentTask;
    bool dictatorFound = false;

    // Перевіряємо чи не має в системі готового до роботи потоку з пріорітетом
    // Для цього проходимо циклом по всіх потоках та шукаємо нульовий потік
    for(uint8_t i = 0; i < taskCount; i++) {
        if(tasks[i].priority == 0 && tasks[i].ready) {
        	// Якщо знаходимо потік диктатор, який готовий до роботи то:
            nextTask = i; // ставимо його на виконання
            dictatorFound = true; // змінюємо статус змінної, яка означає, що диктатор знайдений
            break; // Негайно зупиняємо і потік диктатор назад бере собі владу над процесором
        }
    }

    // А ця велика та жалхива пердуляція робить так:
    /*
	Наприклад, є черга потоків, яка вигладає так: Потік1, Потік2, Потік3.
	Коли у Потоку1 закінчується час, цикл перевірить 2 потік. Припустимо що він спить, значить йдемо далі.
	Далі іде 3 потік і система бере його.
	Таким чином процесорний час ділиться між потоками.
    */
    if (!dictatorFound && (tasks[currentTask].ticksLeft == 0 || !tasks[currentTask].ready)) {
        tasks[currentTask].ticksLeft = tasks[currentTask].timeSlice;
        uint8_t checked = 0;
        do {
            nextTask = (nextTask + 1) % taskCount;
            checked++;
            if (checked >= taskCount) break; 
        } while(!tasks[nextTask].ready || tasks[nextTask].priority == 0 || tasks[nextTask].suspended);
    }

    // Оновлюємо змінну, встановлюючи новий потік на виконання
    currentTask = nextTask;
    // Беремо адресу його стека, щоб потім його виконувати
    current_sp_16 = (uint16_t)tasks[currentTask].sp;
}

// Кожну мілісекунду, коли спрацьовує системний таймер номер 0(від якого також залежить milis())
ISR(TIMER0_COMPA_vect, ISR_NAKED) {
	// Зберігаємо регісти процесора в ОЗП
    SAVE_CONTEXT();
    
    asm volatile (
    	// Читаємо 1(low) та 2(high) байт вказівника стека у регістри 24 та 25(він займає 16 біт тобто 2 байти).
    	// Тепер в цих регістрах значенн цього вказівника.
        "in r24, __SP_L__ \n\t"
        "in r25, __SP_H__ \n\t"
        // Беремо значення значення з регістрів та записуємо в змінну current_sp_16, знову ж таки кожен байт окремо.
        "sts current_sp_16, r24 \n\t"
        "sts current_sp_16+1, r25 \n\t"
    );

    asm volatile (
        "ldi r24, lo8(global_isr_stack + 59) \n\t"
        "ldi r25, hi8(global_isr_stack + 59) \n\t"
        "out __SP_H__, r25 \n\t"
        "out __SP_L__, r24 \n\t"
    );

    // Викликаємо функцію system_tick
    asm volatile ("call system_tick");

    asm volatile (
    	// Читаємо з ОЗП вказівник на стека у регістри 24 та 25.
    	// Тепер там записано вказівник стека.
        "lds r24, current_sp_16 \n\t"
        "lds r25, current_sp_16+1 \n\t"
        // Примусово змінюємо апаратний Stack Pointer. (спочатк пишемо 2 байт потім 1, така особливість процесора)
        "out __SP_H__, r25 \n\t"
        "out __SP_L__, r24 \n\t"
    );
    
    // Записуємо в процесор збережені регістри
    RESTORE_CONTEXT();

    // Асемблерна команда, яка одночасно і вмикає назад переривання і запускає виконання коду з адреси стека.
    asm volatile ("reti");
}

// Ця фукнція викликається тоді, коли якийся потік завершив роботу не коректно
void os_thread_exit_trap(void) {
    panic("Thread exited incorrectly!");
}

bool createThread(void (*taskFunc)(void), uint8_t priority, uint8_t* stack, uint16_t stackSize, uint8_t id) {
    cli(); // Зупиняємо усі операції окрім цієї, щоб ніхто не зміг перервати процес створення потоку

    // Перевірка на занадто велику кількість потоків
    if (taskCount >= MAX_TASKS) { // Якщо так, то
    	sei(); // Дозволити виконувати усі операції знову
    	return false; // Поверути false, щоб показати відмову
    }

    // Захист від нульового покажчика
    if (taskFunc == NULL || stack == NULL) {
        panic("createThread: Null pointer!");
    }

    // Перевіряємо чи не буде стек потоку занадто малим
    if (stackSize < 60) {
        panic("createThread: Stack too small!");
    }

    // Перевірка чи не має потоків з однаковими ID та чи існує вже нульовий потік.
    for (uint8_t i = 0; i < taskCount; i++) {
        if (tasks[i].id == id) {
            panic("createThread: Duplicate Task ID!");
        }
        if (priority == 0 && tasks[i].priority == 0) {
        	// Дозволено лише 1 нульовий потік
            panic("createThread: Cannot be more than one thread with priority 0!");
        }
    }

    //
    // Збираємо пам'ять потока з нуля
    //

    // Заповнюємо увесь стек спеціальними символами
    // Якщо жодного цього символа не залишиться в стеку - значить він вийшов за межі своєї пам'яті.
	for(uint16_t i = 0; i < stackSize; i++) {
        stack[i] = STACK_CANARY;
    }
    // Вказуємо дно стеку
	tasks[taskCount].stackBottom = &stack[0];
	// Вказуємо розмір стеку
    tasks[taskCount].stackSize = stackSize;

    // Робимо вказівник стека (stack + stackSize - 1) = найвищий доступний байт стека
    uint8_t* sp = stack + stackSize - 1;

    // На дно стека кладемо ардесу функції захисту, яка працює при не коректному завершенні роботи потока.
    // Адреса функцї займає 2 байти(бо це 16 біт, а процесор всого на 8), тому записуємо її 2 частинами,
    // причому спочатку останню частину, а потім першу, бо процесор спочатку читає зі стека старший байт, а потім молодший.
    // *sp-- означає записати значення за адресою та одразу відняти один.
    // Наприклад було stack[99] а після запису туди даних стало stack[98]
    *sp-- = ((uint16_t)os_thread_exit_trap) & 0xFF;
    *sp-- = (((uint16_t)os_thread_exit_trap) >> 8) & 0xFF;
    
    // Далі кладемо адресу фукнції з якої починається поток
    *sp-- = ((uint16_t)taskFunc) & 0xFF;
    *sp-- = (((uint16_t)taskFunc) >> 8) & 0xFF;
    
    *sp-- = 0x00; // Порожнє місце для регістра r0.
    // Статусний регістр SREG. Відповідає за увімкнення переривань.
    // Штучно вмикаємо його, щоб коли потік запуститься, переривання працювали.
    *sp-- = 0x80;
    
    // 31 раз записуємо 0x00 - це порожні значення для регістрів від r1 до r31.
    // Потоку вони поки не потрібні, але Планувальник буде їх діставати, тому ми маємо підкласти туди хоч щось, наприклад нулі.
    for(int i = 0; i < 31; i++) *sp-- = 0x00;

	// Записуємо значення sp для цього потоку
    tasks[taskCount].sp = sp;
	
	// Записуємо пріорітет потоку
    tasks[taskCount].priority = priority;

    // Записуємо id потоку
    tasks[taskCount].id = id;
    
    // Визначаємо розмір кванту (скільки часу потоку дозволено безперерво займати ядро)
    // Для задач з пріорітеторм 1 - 15ms
    // з пріорітетом 2 - 10ms
    // з пріорітетом 3 - 5ms
    if (priority == 1) tasks[taskCount].timeSlice = 15;
    else if (priority == 2) tasks[taskCount].timeSlice = 10;
    else if (priority == 3) tasks[taskCount].timeSlice = 5;

    // Якщо пріорітет 0(або інше значення), то цей поток має право брати процесор монопольно на невизначений термін.
    // Нескінченність тут вказати не можна, тому просо пишемо максимальне значення для 8-бітної змінної - 255. 
    else tasks[taskCount].timeSlice = 255;
    

    tasks[taskCount].ticksLeft = tasks[taskCount].timeSlice;

    // Встановлюємо що поток готовий до виконання за замовчуванням
    tasks[taskCount].ready = true;
    // Встановлюємо що потік не зупинено за замовчуванням
    tasks[taskCount].suspended = false;

    // Час, скільки потік повинен спати. Зараз він повинен бути запущений тому пишемо 0.
    tasks[taskCount].wakeUpTime = 0;
    
    taskCount++; // Додаємо 1 до загальної кількості потоків
    sei(); // Продовжуємо виконання усіх задач

    // Успішно
    return true;
}

void os_init(void) {
    // init() з arduino core вже запустила систмений таймер номер 0.
    // Встановлюємо переривання Timer0 на середину його циклу.
    // Тобто коли лічильник доходить до 128, то спрацьовує наше переривання ISR(TIMER0_COMPA_vect)
    // Це зроблено для того, щоб цей планувальник та вбудована в arduino core функція millis не спрацювали в одну мікросекунду,
    // бо ОС ділить системний таймер номер 0 разом з функцією millis()
    OCR0A = 128;
    TIMSK0 &= ~(1 << OCIE0A); // Поки що вимикаємо цю мітку
}

void os_start(void) {
	// Перевірка чи створені взагалі якісь потоки
	if (taskCount == 0) {
        panic("No task found!");
    }

    cli(); // Зупиняємо усі переривання в процесорі.
    // Ставимо значення потоку який зараз виконується на 0,
    // щоб система після запуску почала виконувати потік під номером 0.
    currentTask = 0;
    // Беремо його покажчик
    current_sp_16 = (uint16_t)tasks[0].sp;
    
    // Вмикаємо мітку(своє кастомне переривання TIMER0_COMPA) назад.
    TIMSK0 |= (1 << OCIE0A); 
    
    asm volatile (
        // Читаємо з ОЗП вказівник на стека у регістри 24 та 25.
        // Тепер там записано вказівник стека.
        "lds r24, current_sp_16 \n\t"
        "lds r25, current_sp_16+1 \n\t"
        // Примусово змінюємо апаратний Stack Pointer. (спочатк пишемо 2 байт потім 1, така особливість процесора).
        "out __SP_H__, r25 \n\t"
        "out __SP_L__, r24 \n\t"
    );

    // Записуємо в процесор збережені регістри.
    RESTORE_CONTEXT();
    // Асемблерна команда, яка одночасно і вмикає назад переривання і запускає виконання коду з адреси стека.
    asm volatile("reti");
}


// Функції для дій з потоками:

// Призупинити потік
void suspendThread(uint8_t id) {
    int idx = findThreadById(id);
    if (idx == -1) return; // Такого потоку немає

    uint8_t oldSREG = SREG;
    cli();
    
    tasks[idx].suspended = true;
    tasks[idx].ready = false; // Зупиняємо потік
    
    SREG = oldSREG;

    if (idx == currentTask) {
        thread_delay(1); 
    }
}

// Відновити потік
void resumeThread(uint8_t id) {
    int idx = findThreadById(id);
    if (idx == -1) return;

    uint8_t oldSREG = SREG;
    cli();
    
    tasks[idx].suspended = false;
    // Вмикаємо потік назад, якщо у нього немає таймера на зупинку.
    if (sys_millis >= tasks[idx].wakeUpTime) {
        tasks[idx].ready = true;
    }
    
    SREG = oldSREG;
}


// Геттери, сеттери
uint8_t getThreadCount(void) {
	return taskCount;
}

uint8_t getCurrentPriority(void) {
	return tasks[currentTask].priority;
}

int findThreadById(uint8_t id) {
    for(uint8_t i = 0; i < taskCount; i++) {
        if(tasks[i].id == id) {
            return i;
        }
    }
    return -1;
}

uint8_t getThreadIdByIndex(uint8_t index) {
    if (index >= taskCount) return 0;
    return tasks[index].id;
}
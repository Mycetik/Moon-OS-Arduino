#include <MoonOS.h> // Встановлюємо, що цей файл залежить від ОС.
#include "programs/system_monitor/system_monitor.h" // Підключаємо програму системний монітор.
#include "programs/async_print/async_print.h" // Підключаємо програму яка додає потокобезпечний print.

// Створюємо стеки для потоків
// Ми виділяємо по 10 байт кожному стеку, але архітектура ОС вимагає мінімум 93 байт.
// Тому за кадром ОС додає ще 93 байт до цього об'єму і виходить по 103 байт на кожен потік.
CREATE_STACK(stackTask1, 10); 
CREATE_STACK(stackTask2, 10);

const int led1 = 4;
const int led2 = 5;

void taskBlink1() {
    while(1) {
        // Цей print читає текст прямо з flash пам'яті і повністю потокобезпечний.
        async_print_P(F("Task 1: LED ON\r\n"));
        digitalWrite(led1, HIGH);
        // Це спецальний delay який треба використовувати в потоках замість звичайного.
        // Можна звісно і звичайний використати, але він створює більше навантаження на систему.
        // Також є thread_millis яку бажано писати замість звичайного millis.
        thread_delay(500);
        
        async_print_P(F("Task 1: LED OFF\r\n"));
        digitalWrite(led1, LOW);
        thread_delay(500);
    }
}

void taskBlink2() {
    // В цьому циклі крутиться поток. Перед циклом можна те, що повинно вконуватись один раз.
    while(1) {
        // Цей print читає текст вже з ОЗП, але він все ще потокобезпечний
        async_print("Task 2: LED ON\r\n");
        digitalWrite(led2, HIGH);
        thread_delay(500);
        
        async_print("Task 2: LED OFF\r\n");
        digitalWrite(led2, LOW);
        thread_delay(500);
    }
}

void if_panic(){
    // Вимикаємо усі світлодіоди.
    // Тут не працюють половина команд по типу print, delay і подібні, бо ОС в стані паніки.
    // Але можна вимкнути усю електроніку для безпеки.
    digitalWrite(led2, LOW);
    digitalWrite(led1, LOW);
}

void setup(void) {
    // Цей код виконується в той момент, коли ОС готова до запуску, але її ще не запущено.
    // Тут можна написати наприклад які потоки будуть запущені в системі після її старту або ініціалізувати порти.

    pinMode(led1, OUTPUT);
    pinMode(led2, OUTPUT);

    // Запускаємо програми та потоки
    // Порядок запуску не дуже важливий, якщо вони запускаються до запуску ОС.
    asyncPrint_init(); // Запускаємо службу друку тексту як окремий потік.
    sysmon_enable(2000); // Запускаємо системний монітор.

    // Створюємо робочі потоки
    createThread(taskBlink1, 2, stackTask1, sizeof(stackTask1), 10);
    createThread(taskBlink2, 2, stackTask2, sizeof(stackTask2), 20);

    // Можна зупинити певний потік за допомогою suspendThread(id), а потім відновити його через resumeThread(id)

    // Вказуємо, що робити системі(а точніше яку функцію викликати) якщо ядро зловило критичну помилку і панікує
    setPanicHook(if_panic);
    // До речі ми можемо самі викликати паніку ось так: panic("причина");


    // Після цього коду іде запуск ОС. 
}

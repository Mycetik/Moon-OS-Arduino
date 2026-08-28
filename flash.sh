#!/bin/bash

PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1)

if [ -z "$PORT" ]; then
    echo "Arduino не знайдено!"
    echo "Press any key to continue..."
    read -n 1 -s -r
    exit 1
fi
echo "Знайдено Arduino на порту: $PORT"

fuser -k $PORT 2>/dev/null || true
sleep 1

echo "Запуск Make..."
make clean
make


if [ $? -eq 0 ]; then
    echo "Прошивка мікроконтролера..."
    make upload PORT=$PORT
    echo "Прошивка успішно завершена!"
    make clean

else
    echo "помилка під час компіляції!"
    make clean
fi

echo "Натисніть будь-яку клавішу для відкриття порта 115200..."
read -n 1 -s -r

screen $PORT 115200

echo "Press any key to continue..."
read -n 1 -s -r
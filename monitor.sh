#!/bin/bash

PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1)

if [ -z "$PORT" ]; then
    echo "Arduino не підключена!"
    exit 1
fi

echo "Підключаємось до $PORT 115200"

fuser -k $PORT 2>/dev/null || true
screen $PORT 115200

echo "Press any key to continue..."
read -n 1 -s -r
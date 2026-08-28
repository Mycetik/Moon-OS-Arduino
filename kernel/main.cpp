#include <MoonOS.h>
#include <version.h>

extern HardwareSerial Serial; 

int main(void)
{
    init();
    
#if defined(USBCON)
    USBDevice.attach();
#endif

    Serial.begin(115200);
    Serial.println("Arduino success init");
    Serial.print(2048-getFreeRam());
    Serial.print("/2048 mem\r\n");
    os_init();
    Serial.print("Startup MoonOS Arduino ");
    Serial.print(release);
    Serial.print("...\r\n");

    setup();

    os_start();

    return 0;
}
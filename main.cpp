
#include "mbed.h"
#include "PeripheralPins.h"

#include <cstring>

//------------------------------------
// Hyperterminal configuration
// 115200 bauds, 8-bit data, no parity
//------------------------------------

#define DEVICE_TX D5
#define DEVICE_RX D4

Serial pc(SERIAL_TX, SERIAL_RX, 115200);
Serial device(DEVICE_TX, DEVICE_RX, 115200);

DigitalOut vdd(D6);
DigitalOut gnd(D7);


char deviceRxBuffer[5+69+1];
volatile int  deviceRxCount = 0;
volatile int  deviceTxCount;
void deviceRxISR() {
    if (deviceRxCount+1 == deviceTxCount) // Tristate Tx if last byte
        pin_function(DEVICE_TX, STM_PIN_DATA(STM_MODE_INPUT, GPIO_PULLUP, 0));
    
    while (device.readable() && deviceRxCount < sizeof(deviceRxBuffer)-1) {
        deviceRxBuffer[deviceRxCount++] = device.getc();
    }
    deviceRxBuffer[deviceRxCount] = '\0';
    
    return;
}

void halfDuplexSend(Serial *serial, PinName tx, const char *outBuffer, int nb)
{   
    pinmap_pinout(tx, PinMap_UART_TX);
    
    __disable_irq();
    deviceRxCount = 0;
    deviceTxCount = nb;
    deviceRxBuffer[deviceRxCount] = '\0';
    __enable_irq();
    
    for (int i = 0; i < nb; i ++) {
        serial->putc(outBuffer[i]);
        while (!serial->writeable());
    }
    
}

void printHex(Serial *serial, const char *start, const char *hex, int nb, const char *end) {
    serial->puts(start);
    for (int i = 0; i < nb; i ++)
        serial->printf("%02X ", (int)hex[i]);
    serial->puts(end);
}

int main()
{
    // Frame definition and buffer declaration
    const char tmp107_address_init[]     = {0x55, 0x95, 0x05, '\0'};
    const char tmp107_last_device_poll[] = {0x55, 0x57,       '\0'};
    const char tmp107_global_reset[]     = {0x55, 0x5D,       '\0'};
    const char tmp107_globread_temp[]    = {0x55, 0x03, 0xA0, '\0'};
    const char tmp107_globread_dieid[]   = {0x55, 0x03, 0xAF, '\0'};
    
    // Power the sensor
    vdd = 1;
    gnd = 0;
    
    // Hello !
    pc.puts("\n\r--\tHello World !\n\r");
    device.attach(&deviceRxISR, Serial::RxIrq);
    
    // Initialize sensor addresses
    halfDuplexSend(&device, DEVICE_TX, tmp107_address_init, sizeof(tmp107_address_init)-1);
    wait(0.3); printHex(&pc, "Address init:       ", deviceRxBuffer, deviceRxCount, "\n\r");
    
    // Last Device Poll
    halfDuplexSend(&device, DEVICE_TX, tmp107_last_device_poll, sizeof(tmp107_last_device_poll)-1);
    wait(0.01); printHex(&pc, "Last Device Pool:   ", deviceRxBuffer, deviceRxCount, "\n\r");
    
    // Global Software Reset
    halfDuplexSend(&device, DEVICE_TX, tmp107_global_reset, sizeof(tmp107_global_reset)-1);
    wait(0.01); printHex(&pc, "Global Soft Reset:  ", deviceRxBuffer, deviceRxCount, "\n\r");
    
    // Last Device Poll
    halfDuplexSend(&device, DEVICE_TX, tmp107_last_device_poll, sizeof(tmp107_last_device_poll)-1);
    wait(0.01); printHex(&pc, "Last Device Pool:   ", deviceRxBuffer, deviceRxCount, "\n\r");
    
    // Global Read Die ID
    halfDuplexSend(&device, DEVICE_TX, tmp107_globread_dieid, sizeof(tmp107_globread_dieid)-1);
    wait(0.01); printHex(&pc, "Global Read Die ID: ", deviceRxBuffer, deviceRxCount, "\n\r");
    
    pc.puts("Global Read Impro:  55 03 AZ FU CK\n\r");
    
    int i = 10;
    while(--i) {
        // Global Read Temperature and Print
        halfDuplexSend(&device, DEVICE_TX, tmp107_globread_temp, sizeof(tmp107_globread_temp)-1);
        wait(0.01); printHex(&pc, "Global Read Temp:   ", deviceRxBuffer, deviceRxCount, "  ->  ");
        float temperature = ((int)deviceRxBuffer[4]<<6 | (int)deviceRxBuffer[3]>>2)*0.015625;
        if (temperature >= 128) temperature = temperature-256;
        pc.printf("Temperature: %.2f\n\r", temperature);
        
        wait(1.0);
    }
}

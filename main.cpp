/*
 * Claire Kraft
 * OCT 16 2022
 * This program reads the temperature and the pressure from the BMP180 sensor
 * using I2C and ISRs
 */

#include "CKraft_binaryutils.h"
#include "USBSerial.h"
#include "mbed.h"
#include <I2C.h>
#include <math.h>

#define set (uint32_t *)0x50000508
#define clear (uint32_t *)0x5000050C
#define dir (uint32_t *)0x50000514
#define ping (uint8_t)16
#define pinr (uint8_t)24
#define pinb (uint8_t)6


// digital out pins
DigitalOut TurnOn(p22);
DigitalOut TurnOn2(p32);
DigitalOut TurnOn3(p14);
DigitalOut TurnOn4(p15);
DigitalOut GreenLed(p16);


EventFlags event_flags;
Mutex Mutex1;
Ticker flipper;
Thread thread1;
Thread thread2;
USBSerial SERIAL;
char Paddress = 0x39;
uint8_t x = 0, y = 1;
char Waddress = (0x39 <<1) | (x);
char Raddress = (0x39 <<1) | (y);
I2C i2c(p14, p15);

typedef struct {
    int Proximity;
} mail_t;

Mail<mail_t, 16> mail_box;

// Thread that takes the proximity value and shines LED based on that
void LED(){
    // initialize
    int prox;
    PwmOut led1(LED_BLUE);
    led1.period_ms(50);
    float dutyc = 1.0;
    float period = 50;
    float light;
    float dark;
    
    while (true) {
        // when mail box is not empty do this
        if (!mail_box.empty()) {

            //get the mail from the mail box
            mail_t *mail = mail_box.try_get();
            prox = mail->Proximity;
            // free the mail so you don't run out of storage
            mail_box.free(mail);

        }
        // Printing out the calibrated values
        SERIAL.printf("Message was received = %i\r\n", prox);
        if(prox == 0){
            SERIAL.printf("Hand out of range!\r\n");
        }
        else if(prox == 1){
            SERIAL.printf("Hand is about 12 cm away.\r\n");
        }
        else if(prox == 2){
            SERIAL.printf("Hand is about 11 cm away.\r\n");
        }
        else if(prox == 3){
            SERIAL.printf("Hand is about 10 cm away.\r\n");
        }
        else if(prox == 4){
            SERIAL.printf("Hand is about 9 cm away.\r\n");
        }
        else if(prox == 5 || prox == 6){
            SERIAL.printf("Hand is about 8 cm away.\r\n");
        }
        else if(prox > 6 && prox <= 10){
            SERIAL.printf("Hand is about 7 cm away.\r\n");
        }
        else if(prox > 10 && prox <= 15){
            SERIAL.printf("Hand is about 6 cm away.\r\n");
        }
        else if(prox > 15 && prox <= 25){
            SERIAL.printf("Hand is about 5 cm away.\r\n");
        }
        else if(prox > 25 && prox <= 50){
            SERIAL.printf("Hand is about 4 cm away.\r\n");
        }
        else if(prox > 50 && prox <= 100){
            SERIAL.printf("Hand is about 3 cm away.\r\n");
        }
        else if(prox > 100 && prox <= 200){
            SERIAL.printf("Hand is about 2 cm away.\r\n");
        }
        else if(prox >= 200){
            SERIAL.printf("Hand is about 1 cm away.\r\n");
        }
        else{
            SERIAL.printf("ERROR.\r\n");
        }
        // changing the LEDs from the proximity measurement
        if (prox == 0){
            GreenLed = 0;
            dutyc = 1;
        }
        else{
            GreenLed = 1;
            dutyc = 1 - prox/75.0;
            if (dutyc < 0){
                dutyc = 0; 
            }
        }
        led1.write(dutyc);
        thread_sleep_for(100);
    }

}

// measures the proximity of an object from the APDS-9960 sensor
void proximity(){
    // checking i the controller is communicating to the right sensor
    char value[1];
    char out[1];
    value[0] = 0x92;
    out[0] = 0x00;
    thread_sleep_for(10);
    i2c.write(Waddress, value, 1, true);
    i2c.read(Raddress, out, 1, false);
    if (out[0] == 0xAB) {
        SERIAL.printf("RIGHT sensor!!\r\n");
    } else {
        SERIAL.printf("WRONG sensor\r\n");
        SERIAL.printf("Reading is %d\r\n", out[0]);
    }

    
    char output3[1];
    char input1[2], input2[2], input3[2];
    Mutex1.lock();
    //setting the enable register
    input1[0] = 0x80;
    input1[1] = 0x25;
    SERIAL.printf("the input is %d\r\n", input1[1]);
    if (i2c.write(Waddress, (const char*)input1, 2, false) != 0){
        SERIAL.printf("Failed to update\r\n");
    }
        
    // setting the Configuration Register Two for the LED_BOOST   300% 
    input2[0] = 0x90;
    input2[1] = 0x31;
    SERIAL.printf("the input is %d\r\n", input2[1]);
    if (i2c.write(Waddress, (const char*)input2, 2, false) != 0){
           SERIAL.printf("Failed to update\r\n");
    }

    //Setting the Control Register One for the LDrive 100mA
    input3[0] = 0x8F;
    input3[1] = 0x00;
    SERIAL.printf("the input is %d\r\n", input3[1]);
    if (i2c.write(Waddress, (const char*)input3, 2, false) != 0){
        SERIAL.printf("Failed to update\r\n");
    }
    Mutex1.unlock();

    while(true){

        Mutex1.lock();
        // Reading proximity data from the sensor
        output3[0] = 0x9C;
        i2c.write(Waddress, output3, 1, true);
        i2c.read(Raddress, output3, 1, false);
        //SERIAL.printf("proximity is %d\r\n", output3[0]);
        Mutex1.unlock();

        //sending the prox to the mailbox
        mail_t *mail = mail_box.try_alloc();
        mail->Proximity = output3[0];
        mail_box.put(mail);

        thread_sleep_for(1000);

    }

} 


// main() runs in its own thread in the OS
int main()
{
    // setting high the pin 22 on port 0  is this port 22??
    TurnOn = 1;
    TurnOn2 = 1;
    TurnOn3 = 1;
    TurnOn4 = 1;


    //testing the write and read values
    SERIAL.printf("write address is %d\r\n", Waddress);
    SERIAL.printf("read address is %d\r\n", Raddress);


    thread1.start(proximity);
    thread2.start(LED);

    while (true) {
        thread_sleep_for(1000);
    }
}


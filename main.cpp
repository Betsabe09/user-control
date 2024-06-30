#include "mbed.h"
#include "stdint.h"
#define MAX_FAILED 10

DigitalIn buttonUser1(D4);
DigitalIn buttonUser2(D5);
DigitalIn buttonUser3(D3);

DigitalOut ledUser1(D14);
DigitalOut ledUser2(D15);

Timer timer;

static UnbufferedSerial serialComm (D1, D0);

bool buttonPressed1 = false;
bool buttonPressed2 = false;
bool buttonPressed3 = false;

bool receiveMsg = false;
bool confirmationReceived = false;

bool overtime = false;

int communicationFailedCount = 0;

enum States{
    MONITOR,
    PANIC,
    OFF
};

States state;
States last_state;

auto start = 0;

char checkMsg = '\0';
char userMonitorMsg = 'm';
char userPanicMsg = 'p';

bool serialCommunicationLoop (char, char); 

void buttonsRead (void);

void monitorState (void);
void panicState (void);
void offState (void);

int main()
{
    buttonUser1.mode(PullUp);
    buttonUser2.mode(PullUp);
    buttonUser3.mode(PullUp);

    serialComm.baud(9600);
    serialComm.format(
        /* bits */ 8,
        /* parity */ SerialBase::None,
        /* stop bit */ 1
    );

    state = OFF;

    while (true) {
        buttonsRead();
        switch (state){
            case 0:
                monitorState();
                break;
            case 1:
                panicState();
                break;
            case 2:
                offState();
                break;
        }
    }
}

void buttonsRead (void){

    if (!buttonUser1 && !buttonPressed1) {
        last_state = state;
        state = MONITOR;
        buttonPressed1 = true;
    }else if (buttonUser1){
        buttonPressed1 = false;
    }

    if(!buttonUser2 && !buttonPressed2) {
        last_state = state;
        state = PANIC;
        buttonPressed2 = true;
    }else if (buttonUser2){
        buttonPressed2 = false;
    }
    
    if (!buttonUser3 && !buttonPressed3){
        last_state = state;
        state = OFF;
        buttonPressed3 = true;
    }else if (buttonUser3){
        buttonPressed3 = false;
    }
}

void monitorState (void){
    if (last_state == PANIC){
        receiveMsg = false;
        timer.stop();
        last_state = state;
    }

    if (serialCommunicationLoop(userMonitorMsg, 'M')){
        overtime = false;
        timer.start();
        start = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
    }

    if (!receiveMsg){
        serialComm.write(&userMonitorMsg, 1);
        receiveMsg = true;
        timer.start();
        start = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
    } else if (serialComm.readable()){
        serialComm.read(&checkMsg, 1);
        if (checkMsg == 'M'){
            communicationFailedCount = 0;
            overtime = false;
            start = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
        }else {
            communicationFailedCount ++;
        }
        receiveMsg = false;
    }

    if ((communicationFailedCount > MAX_FAILED) || overtime){
        ledUser2 = ledUser1;
        if (chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count() > (start + 1000)){
            ledUser1 = !ledUser1;
            ledUser2 = !ledUser2;
            start = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
        }
       // ledUser2 = 1;
       // ledUser1 = 1;
    }else {
        ledUser1 = 1;
        ledUser2 = 0;
        if (chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count() > (start + 5000)){
            overtime = true;
            start = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
        }
    }
}

void panicState (void){
    if (last_state == MONITOR){
        receiveMsg = false;
        confirmationReceived = false;
        communicationFailedCount = 0;
        timer.stop();
        last_state = state;
    }
    if (!confirmationReceived){
        if (!receiveMsg){
            serialComm.write(&userPanicMsg, 1);
            receiveMsg = true;
            timer.start();
            start = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();    
        } else if (serialComm.readable()){
            serialComm.read(&checkMsg, 1);
            if (checkMsg == 'P'){
                confirmationReceived = true;
                communicationFailedCount = 0;
                overtime = false;
                timer.stop();
            }else {
                communicationFailedCount ++;
                }
            receiveMsg = false;
        }
    }

    if ((communicationFailedCount > MAX_FAILED) || overtime){
        ledUser2 = ledUser1;
        if (chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count() > (start + 1000)){
            ledUser1 = !ledUser1;
            ledUser2 = !ledUser2;
            start = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
        }
       // ledUser2 = 1;
       // ledUser1 = 1;
    }else {
        ledUser2 = 1;
        ledUser1 = 0;
        if (chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count() > (start + 5000)){
            overtime = true;
            start = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
        }
    }
}

void offState (void){
    ledUser1 = 0;
    ledUser2 = 0;
    communicationFailedCount = 0;
    confirmationReceived = false;
    receiveMsg = false;
    timer.stop();
}
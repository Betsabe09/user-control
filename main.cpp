#include "mbed.h"
#include "stdint.h"

#define MAX_FAILED 10
#define TIMER_FOR_OVERTIME 5000
#define TIMER_BLINK 1000

// Inicialización de pines para botones y LEDs
DigitalIn buttonUser1(D4);
DigitalIn buttonUser2(D5);
DigitalIn buttonUser3(D3);

DigitalOut ledUser1(D14);
DigitalOut ledUser2(D15);

Timer timer;
static UnbufferedSerial serialComm(D1, D0);

// Variables de estado de los botones
bool buttonPressed1 = false;
bool buttonPressed2 = false;
bool buttonPressed3 = false;

// Variables de comunicación y temporización
bool receiveMsg = false;
bool confirmationReceived = false;
bool overtime = false;
int communicationFailedCount = 0; 

enum States { MONITOR, PANIC, OFF };
States state;
States last_state;

// Variables de temporización
auto startTime = 0;
auto startTimeBlink = 0;

// Mensajes de comunicación
char checkMsg = '\0';
char userMonitorMsg = 'm';
char userPanicMsg = 'p';

// Prototipos de funciones
void buttonsRead(void);
void monitorState(void);
void panicState(void);
void offState(void);
void handleButtonPress(DigitalIn &button, bool &buttonPressed, States newState);
bool handleCommunication(char expectedMsg, char sendMsg);
void resetState(void);
void blinkLeds(void);
void handleLeds(DigitalOut &led1, DigitalOut &led2, bool normalState);

int main() {
    // Configuración de los botones con PullUp
    buttonUser1.mode(PullUp);
    buttonUser2.mode(PullUp);
    buttonUser3.mode(PullUp);

    // Configuración del serial
    serialComm.baud(9600);
    serialComm.format(8, SerialBase::None, 1);

    state = OFF;

    while (true) {
        buttonsRead();
        if (state != last_state) {
            resetState();
            last_state = state;
        }
        switch (state) {
            case MONITOR:
                monitorState();
                break;
            case PANIC:
                panicState();
                break;
            case OFF:
                offState();
                break;
        }
    }
}

// Lectura de los botones y actualización del estado
void buttonsRead(void) {
    handleButtonPress(buttonUser1, buttonPressed1, MONITOR);
    handleButtonPress(buttonUser2, buttonPressed2, PANIC);
    handleButtonPress(buttonUser3, buttonPressed3, OFF);
}

// Manejo de la pulsación de un botón
void handleButtonPress(DigitalIn &button, bool &buttonPressed, States newState) {
    if (!button && !buttonPressed) {
        last_state = state;
        state = newState;
        buttonPressed = true;
    } else if (button) {
        buttonPressed = false;
    }
}

// Estado de monitoreo
void monitorState(void) {

    if (!confirmationReceived) {
        confirmationReceived = handleCommunication('M', userMonitorMsg);
    }else{
        confirmationReceived = false;
    }

     handleLeds(ledUser1, ledUser2, true);
}

// Estado de pánico
void panicState(void) {

    if (!confirmationReceived) {
        confirmationReceived = handleCommunication('P', userPanicMsg);
    }else {
        timer.stop();
    }

    handleLeds(ledUser2, ledUser1, true);
}

// Estado apagado
void offState(void) {
    ledUser1 = 0;
    ledUser2 = 0;
    timer.stop();
   // resetState();
}

// Manejo de la comunicación serial
bool handleCommunication(char expectedMsg, char sendMsg) {
    if (!receiveMsg) {
        serialComm.write(&sendMsg, 1);
        receiveMsg = true;
        timer.start();
        startTime = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
    } else if (serialComm.readable()) {
        serialComm.read(&checkMsg, 1);
        if (checkMsg == expectedMsg) {
            communicationFailedCount = 0;
            overtime = false;
            receiveMsg = false;
            return true;
        } else {
            communicationFailedCount++;
        }
        receiveMsg = false;
    } else if (chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count() > (startTime + TIMER_FOR_OVERTIME)) {
        overtime = true;
        receiveMsg = false;
    }
    return false;
}

// Reseteo de variables de estado
void resetState(void) {
    communicationFailedCount = 0;
    overtime = false;
    confirmationReceived = false;
    receiveMsg = false;
   // timer.stop();
}

// Parpadeo de LEDs en caso de error
void blinkLeds(void) {
    if (chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count() > (startTimeBlink + TIMER_BLINK)) {
        ledUser1 = !ledUser1;
        ledUser2 = ledUser1;
        startTimeBlink = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
    }
}

void handleLeds(DigitalOut &led1, DigitalOut &led2, bool normalState) {
    if ((communicationFailedCount > MAX_FAILED) || overtime) {
        blinkLeds();
    } else {
        led1 = normalState;
        led2 = !normalState;
    }
}
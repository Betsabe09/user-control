#include "mbed.h"
#include "stdint.h"

#define MAX_FAILED 10
#define TIMER_FOR_OVERTIME 5000
#define TIMER_BLINK 1000

// Inicialización de pines para botones y LEDs con PullUp
DigitalIn button1(D4, PullUp);
DigitalIn button2(D5, PullUp);
DigitalIn button3(D3, PullUp);

DigitalOut led1(D14);
DigitalOut led2(D15);

// Inicialización del Timer
Timer timer;

// Inicialización del puerto serie
static UnbufferedSerial serialComm(D1, D0);

// Variables de estado de los botones
bool isButton1Pressed = false;
bool isButton2Pressed = false;
bool isButton3Pressed = false;

bool isButtonFlagSet = true;
bool wasButtonFlagSet = false;

// Variables de comunicación y temporización
bool isReceiveMsg = false;
bool confirmationReceived = false;
bool isOvertime = false;
int commFailCount = 0;

enum States { MONITOR, PANIC, OFF };

States currentState = OFF;
States lastState = OFF;

// Variables de temporización
auto startTime = 0;
auto startTimeBlink = 0;

// Prototipos de funciones
void readButtons(void);
void handleMonitorState(void);
void handlePanicState(void);
void handleOffState(void);
void processButtonPress(DigitalIn &button, bool &isButtonPressed, States newState);
bool processCommunication(char expectedMsg, char sendMsg);
void resetStateVariables(void);
void updateLeds(DigitalOut &led1, DigitalOut &led2, bool normalState1, bool normalState2);
void handleStates(void);

int main() {
    // Configuración del serial
    serialComm.baud(9600);
    serialComm.format(8, SerialBase::None, 1);

    while (true) {
        readButtons();
        handleStates();
    }
}

void handleStates(void) {
    if (currentState != lastState) {
        resetStateVariables();
        isButtonFlagSet = true;
        lastState = currentState;
    }

    switch (currentState) {
        case MONITOR:
            handleMonitorState();
            break;
        case PANIC:
            handlePanicState();
            break;
        case OFF:
            if (isButtonFlagSet != wasButtonFlagSet) {
                resetStateVariables();
                if (isButtonFlagSet) {
                    confirmationReceived = true;
                }
                wasButtonFlagSet = isButtonFlagSet;
            }
            handleOffState();
            break;
    }
}

// Lectura de los botones y actualización del estado
void readButtons(void) {
    processButtonPress(button1, isButton1Pressed, MONITOR);
    processButtonPress(button2, isButton2Pressed, PANIC);
    processButtonPress(button3, isButton3Pressed, OFF);
}

// Manejo de la pulsación de un botón
void processButtonPress(DigitalIn &button, bool &isButtonPressed, States newState) {
    if (!button && !isButtonPressed) {
        currentState = newState;
        isButtonFlagSet = !isButtonFlagSet;
        isButtonPressed = true;
    } else if (button) {
        isButtonPressed = false;
    }
}

// Estado de monitoreo
void handleMonitorState(void) {
    if (!confirmationReceived) {
        confirmationReceived = processCommunication('M', 'm');
    } else {
        confirmationReceived = false;
    }
    updateLeds(led1, led2, true, false);
}

// Estado de pánico
void handlePanicState(void) {
    if (!confirmationReceived) {
        confirmationReceived = processCommunication('P', 'p');
    } else {
        timer.stop();
    }
    updateLeds(led1, led2, false, true);
}

// Estado apagado
void handleOffState(void) {
    if (!confirmationReceived) {
        confirmationReceived = processCommunication('O', 'o');
    } else {
        timer.stop();
    }
    updateLeds(led1, led2, false, false);
}

// Manejo de la comunicación serial
bool processCommunication(char expectedMsg, char sendMsg) {
    char checkMsg = '\0';

    if (!isReceiveMsg) {
        serialComm.write(&sendMsg, 1);
        isReceiveMsg = true;
        timer.start();
        startTime = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
    } else if (serialComm.readable()) {
        serialComm.read(&checkMsg, 1);
        if (checkMsg == expectedMsg) {
            commFailCount = 0;
            isOvertime = false;
            return true;
        } else {
            commFailCount++;
        }
        isReceiveMsg = false;
    } else if (chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count() > (startTime + TIMER_FOR_OVERTIME)) {
        isOvertime = true;
        isReceiveMsg = false;
    }
    return false;
}

// Reseteo de variables de estado
void resetStateVariables(void) {
    commFailCount = 0;
    isOvertime = false;
    confirmationReceived = false;
    isReceiveMsg = false;
}

// Parpadeo de LEDs en caso de error
void updateLeds(DigitalOut &led1, DigitalOut &led2, bool normalState1, bool normalState2) {
    if ((commFailCount > MAX_FAILED) || isOvertime) {
        if (chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count() > (startTimeBlink + TIMER_BLINK)) {
            led1 = !led1;
            led2 = led1;
            startTimeBlink = chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count();
        }
    } else {
        led1 = normalState1;
        led2 = normalState2;
    }
}
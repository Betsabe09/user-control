#include "mbed.h"
#include "stdint.h"

#define elapsed_t_s(x)    chrono::duration_cast<chrono::seconds>((x).elapsed_time()).count()

#define TIME_FOR_OVERTIME 5      ///< Tiempo en milisegundos para considerar sobretiempo en la comunicación
#define BLINK_TIME 1             ///< Tiempo en milisegundos para el parpadeo de los LEDs en caso de error
#define LED_ON_OFF_TIME 2        ///< Tiempo en milisegundos para indicar el estado OFF

//=====[Declaration and initialization of public global objects]===============
DigitalIn button1(D4, PullUp);       /**< Botón 1 en el pin D4 */
DigitalIn button2(D5, PullUp);       /**< Botón 2 en el pin D5 */
DigitalIn button3(D3, PullUp);       /**< Botón 3 en el pin D3 */

DigitalOut led1(D14);                /**< LED 1 en el pin D14 */
DigitalOut led2(D15);                /**< LED 2 en el pin D15 */

Timer timer;                         /**< Timer para la temporización general */

static UnbufferedSerial serialComm(PB_10, PB_11); /**< Comunicación serie sin búfer en pines D1 (TX) y D0 (RX) */

//=====[Declaration and initialization of public global variables]=============
bool isButton1Pressed = false;       /**< Estado de presión del botón 1 */
bool isButton2Pressed = false;       /**< Estado de presión del botón 2 */
bool isButton3Pressed = false;       /**< Estado de presión del botón 3 */

bool isButtonFlagSet = true;         /**< Bandera de estado de botón */
bool wasButtonFlagSet = false;       /**< Estado previo de la bandera de botón */

bool ledFlag = true;

bool isReceiveMsg = false;           /**< Bandera de recepción de mensaje */
bool confirmationReceived = false;   /**< Bandera de confirmación recibida */
bool isOvertime = false;             /**< Bandera de sobretiempo */

int isButtonFlagCount = 0;

enum States { MONITOR, PANIC, OFF }; /**< Estados posibles del sistema */

States currentState = OFF;           /**< Estado actual del sistema */
States lastState = OFF;              /**< Último estado del sistema */

auto startTime = 0;                  /**< Tiempo de inicio de temporización */
auto startTimeBlink = 0;             /**< Tiempo de inicio para parpadeo de LEDs */
auto startTimeLed = 0;
//=====[Declarations (prototypes) of public functions]=========================
void readButtons();
/**<
 * Lee el estado de los botones y actualiza las variables de estado.
 * @param none
 * @return none
 */

void handleMonitorState();
/**<
 * Maneja la lógica del estado MONITOR.
 * @param none
 * @return none
 */

void handlePanicState();
/**<
 * Maneja la lógica del estado PANIC.
 * @param none
 * @return none
 */

void handleOffState();
/**<
 * Maneja la lógica del estado OFF.
 * @param none
 * @return none
 */

void processButtonPress(DigitalIn &button, bool &isButtonPressed, States newState);
/**<
 * Procesa la pulsación de un botón.
 * @param button Referencia al botón a procesar.
 * @param isButtonPressed Referencia al estado de presión del botón.
 * @param newState El nuevo estado que se establecerá si el botón es presionado.
 * @return none
 */

bool processCommunication(char expectedMsg, char sendMsg);
/**<
 * Procesa la comunicación serie enviando y recibiendo mensajes.
 * @param expectedMsg Mensaje esperado de respuesta.
 * @param sendMsg Mensaje a enviar.
 * @return true si la comunicación fue exitosa, false en caso contrario.
 */

void resetStateVariables();
/**<
 * Resetea las variables de estado del sistema.
 * @param none
 * @return none
 */

void updateLeds(DigitalOut &led1, DigitalOut &led2, bool normalState1, bool normalState2);
/**<
 * Actualiza el estado de los LEDs.
 * @param led1 Referencia al primer LED.
 * @param led2 Referencia al segundo LED.
 * @param normalState1 Estado normal del primer LED.
 * @param normalState2 Estado normal del segundo LED.
 * @return none
 */

void processStates();
/**<
 * Procesa el estado actual del sistema.
 * @param none
 * @return none
 */

//=====[Main function, the program entry point after power on or reset]========
int main()
/**<
 * Llama a las funciones para inicializar los objetos de entrada y salida, e implementa el comportamiento del sistema.
 * @param none
 * @return none
 */
{
    // Configuración del serial
    serialComm.baud(9600);
    serialComm.format(8, SerialBase::None, 1);
    serialComm.set_blocking(false);
    timer.start();

    while (true) {
        readButtons();
        processStates();
    }
}

void processStates()
{
    if (currentState != lastState) {
        resetStateVariables();
       // wasButtonFlagSet = isButtonFlagSet;
        lastState = currentState;
    }

    switch (currentState) {
        case MONITOR:
            isButtonFlagSet = true;
            wasButtonFlagSet = isButtonFlagSet;
            handleMonitorState();
            break;
        case PANIC:
            isButtonFlagSet = true;
            wasButtonFlagSet = isButtonFlagSet;
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

void readButtons()
{
    processButtonPress(button1, isButton1Pressed, MONITOR);
    processButtonPress(button2, isButton2Pressed, PANIC);
    processButtonPress(button3, isButton3Pressed, OFF);
}

void processButtonPress(DigitalIn &button, bool &isButtonPressed, States newState)
{
    if (!button && !isButtonPressed) {
        currentState = newState;
        isButtonFlagSet = !isButtonFlagSet;
        isButtonPressed = true;
    } else if (button) {
        isButtonPressed = false;
    }
}

void handleMonitorState()
{
    if (!confirmationReceived) {
        confirmationReceived = processCommunication('M', 'm');
    } else {
        confirmationReceived = false;
    }
    updateLeds(led1, led2, true, false);
}

void handlePanicState()
{
    if (!confirmationReceived) {
        confirmationReceived = processCommunication('P', 'p');
    } else {
       // timer.stop();
    }
    updateLeds(led1, led2, false, true);
}

void handleOffState()
{
    bool normalOffState1;
    bool normalOffState2;
   // bool ledFlag;

    if (!confirmationReceived) {
        confirmationReceived = processCommunication('O', 'o');
        normalOffState1 = true;
        normalOffState2 = true;
        ledFlag = true;
    } else {
        if (ledFlag){
            //timer.start();
            //startTimeLed = elapsed_t_s(timer);
            serialComm.write("x", 1);
            timer.reset();
            ledFlag = false;
        }
        if (elapsed_t_s(timer) > LED_ON_OFF_TIME) {
            normalOffState1 = false;
            normalOffState2 = false;
            //timer.stop();
        }else {
            normalOffState1 = true;
            normalOffState2 = true;
        }
    }

    updateLeds(led1, led2, normalOffState1, normalOffState2);
}

bool processCommunication(char expectedMsg, char sendMsg)
{
    char checkMsg = '\0';

    if (!isReceiveMsg) {
        serialComm.write(&sendMsg, 1);
        isReceiveMsg = true;
       // timer.start();
        startTime = elapsed_t_s(timer);
       // timer.reset();
    } else if (serialComm.readable()) {
        serialComm.read(&checkMsg, 1);
        if (checkMsg == expectedMsg || checkMsg == 'P') {
            if (currentState == MONITOR && checkMsg == 'P'){
                lastState = currentState;
                currentState = PANIC;
            }
            isOvertime = false;
            isReceiveMsg = false;
            return true;
        }
    }else if (elapsed_t_s(timer) > (startTime + TIME_FOR_OVERTIME)) {
        isOvertime = true;
        isReceiveMsg = false;
    }
    return false;
}

void resetStateVariables()
{
    isOvertime = false;
    confirmationReceived = false;
    isReceiveMsg = false;
}

void updateLeds(DigitalOut &led1, DigitalOut &led2, bool normalState1, bool normalState2)
{
    if (isOvertime) {
        if (elapsed_t_s(timer) > BLINK_TIME) {
            led1 = elapsed_t_s(timer) % 2;
            led2 = led1;
           // startTimeBlink = elapsed_t_s(timer);
        }
    } else {
        led1 = normalState1;
        led2 = normalState2;
    }
}

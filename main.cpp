/**
 * @file main.cpp
 * @brief Programa que maneja el control de botones del usuario del sistema de alarma.
 * @author BetsabeAilenRodriguez
 */

//=====[Librerías]===========
#include "mbed.h"

//=====[Definición de parámetros de Tiempo y función del tiempo]===========
/**
 * @def elapsed_t_s(x)
 * @brief Macro para obtener el tiempo transcurrido en segundos desde el inicio de un timer.
 * 
 * Esta macro utiliza chrono::duration_cast para convertir el tiempo transcurrido
 * desde el inicio del timer `x` en segundos.
 * 
 * @param x El objeto timer del cual se desea obtener el tiempo transcurrido.
 * @return El tiempo transcurrido en segundos como un valor entero.
 */
#define elapsed_t_s(x)    chrono::duration_cast<chrono::seconds>((x).elapsed_time()).count()

#define TIME_FOR_OVERTIME 5          ///< Tiempo en segundos para considerar sobretiempo en la comunicación
#define BLINK_TIME 1                 ///< Tiempo en segundos para el parpadeo de los LEDs en caso de error
#define LED_ON_OFF_TIME 2            ///< Tiempo en segundos para indicar el estado OFF

//=====[Declaración e inicialización de objetos globales públicos]=============
DigitalIn button1(D4, PullUp);       ///< Botón 1 en el pin D4, correspondiente a la activación de MONITOR
DigitalIn button2(D5, PullUp);       ///< Botón 2 en el pin D5, correspondiente a la activación de PANIC
DigitalIn button3(D3, PullUp);       ///< Botón 3 en el pin D3, correspondiente a la activación de OFF

DigitalOut led1(D14);                ///< LED 1 en el pin D14, correspondiente a la activación de MONITOR
DigitalOut led2(D15);                ///< LED 2 en el pin D15, correspondiente a la activación de PANIC

Timer timer;                         ///< Timer para la temporización general

static UnbufferedSerial serialComm(PB_10, PB_11);  ///< Comunicación serie sin búfer en pines PB_10 (TX) y PB_11 (RX)

//=====[Declaración e inicialización de variables globales públicas]===========
bool isButtonFlagSet = true;          ///< Bandera de estado de botón para setear OFF
bool wasButtonFlagSet = false;        ///< Estado previo de la bandera de botón

bool isButton1Pressed = false;        ///< Estado de presión del botón 1 
bool isButton2Pressed = false;        ///< Estado de presión del botón 2 
bool isButton3Pressed = false;        ///< Estado de presión del botón 3 

bool isReceiveMsg = false;            ///< Bandera de estado para la recepción del mensaje
bool confirmationReceived = false;    ///< Bandera de confirmación de mensaje recibido 
bool isOvertime = false;              ///< Bandera de sobretiempo

/**
 * @enum States
 * @brief Enumera los posibles estados del sistema.
 */
enum States {
    OFF,        ///< Estado apagado 
    MONITOR,    ///< Estado de monitoreo 
    PANIC       ///< Estado de pánico 
};

States currentState = OFF;            ///< Estado actual del sistema, inicialmente OFF
States lastState = OFF;               ///< Último estado del sistema, inicialmente OFF

auto startTime = 0;                   ///< Tiempo de inicio de temporización

//=====[Declaraciones (prototipos) de funciones públicas]=======================
/**
 * @brief Función para procesar los estados del sistema y manejar las transiciones entre ellos.
 *
 * Esta función lee los botones, gestiona las variables de estado y ejecuta las funciones
 * correspondientes según el estado actual del sistema. También gestiona las transiciones
 * entre estados y actualiza las variables de estado necesarias para cada estado.
 *
 * @param none
 * @return void
 */
void processStates(void);
/**
 * @brief Lee el estado de los botones y procesa las acciones correspondientes.
 * 
 * Esta función llama a la función `processButtonPress` para cada botón específico,
 * actualizando el estado de la aplicación según el botón presionado.
 * 
 * @see processButtonPress
 * @param none
 * @return void
 */
void readButtons();

/**
 * @brief Maneja el estado MONITOR: envía 'm' y espera 'M'.
 *
 * Si no se ha recibido una confirmación previa, intenta procesar la comunicación esperando una respuesta específica.
 * Actualiza los LEDs según el tiempo transcurrido.
 *
 * @param none
 * @return void
 */
void handleMonitorState();

/**
 * @brief Maneja el estado PANIC: envía 'p' y espera 'P'.
 *
 * Si no se ha recibido una confirmación previa, intenta procesar la comunicación esperando una respuesta específica.
 * Actualiza los LEDs según el tiempo transcurrido.
 *
 * @param none
 * @return void
 */
void handlePanicState();

/**
 * @brief Maneja el estado OFF: envía 'o' y espera 'O' en cuyo caso se solicite apagar un estado anterior.
 * 
 * Este estado, tiene dos funcionalidades, como OFF para apagar el estado anterior o como OFF para finalizar la comunicación.
 * Si no se ha recibido una confirmación previa, intenta procesar la comunicación esperando una respuesta específica.
 * Actualiza los estados normales y el estado del LED según el tiempo transcurrido.
 *
 * @param none
 * @return void
 */
void handleOffState();

/**
 * @brief Procesa la pulsación de un botón y actualiza el estado del sistema según el botón presionado.
 * 
 * Si el botón está presionado y no se ha registrado como presionado previamente, actualiza el estado
 * actual ("currentState") al nuevo estado especificado ("newState"), y marca el botón como presionado.
 * 
 * Si el botón se libera, marca el botón como no presionado.
 * 
 * @param button Referencia a un objeto `DigitalIn` que representa el botón a monitorear.
 * @param isButtonPressed Referencia a una variable booleana que indica si el botón está presionado.
 * @param newState Estado al que se transicionará si el botón es presionado.
 * @return void
 */
void processButtonPress(DigitalIn &button, bool &isButtonPressed, States newState);

/**
 * @brief Procesa la comunicación serie esperando un mensaje determinado y enviando un mensaje paricular.
 *
 * Esta función gestiona la comunicación serie enviando un mensaje y esperando una respuesta 
 * específica dentro de un tiempo determinado. Si se recibe el mensaje esperado, actualiza el 
 * estado del sistema según sea necesario. 
 *
 * @param expectedMsg Mensaje esperado.
 * @param sendMsg Mensaje a enviar.
 * @return true si se recibió el mensaje esperado, false si no.
 */
bool processCommunication(char expectedMsg, char sendMsg);

/**
 * @brief Reinicia las variables de estado.
 * @param none
 * @return void
 */
void resetStateVariables();

/**
 * @brief Actualiza el estado de los LEDs según las condiciones actuales.
 * 
 * Si el sistema está en sobretiempo, los LEDs parpadean a intervalos definidos.
 * De lo contrario, los LEDs se establecen en los estados normales proporcionados.
 * 
 * @param led1 Referencia a DigitalOut para el LED 1.
 * @param led2 Referencia a DigitalOut para el LED 2.
 * @param normalState1 Estado normal del LED 1.
 * @param normalState2 Estado normal del LED 2.
 */
void updateLeds(DigitalOut &led1, DigitalOut &led2, bool normalState1, bool normalState2);

//=====[Función principal, el punto de entrada del programa después de encender o resetear]========
/**
 * @brief Función principal del programa.
 *
 * Esta función inicializa los componentes necesarios, configura el puerto serial
 * y el temporizador, y luego entra en un bucle infinito donde procesa los estados
 * del sistema.
 *
 * @return int El valor de retorno representa el éxito de la aplicación.
 */
int main()
{
    // Inicialización del puerto serial
    serialComm.baud(9600);  // Configura la velocidad de baudios a 9600
    serialComm.set_blocking(false);  // Configura la comunicación serial como no bloqueante

    timer.start();  // Inicia el temporizador

    while (true) {
        processStates();  // Llamada a la función que maneja los estados del sistema
    }
}

void processStates()
{
    readButtons();
    if (currentState != lastState) {
        resetStateVariables();
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
    }
    updateLeds(led1, led2, false, true);
}

void handleOffState()
{
    static bool normalOffState1 = true;
    static bool normalOffState2 = true;
    static bool ledFlag = true;

    if (!confirmationReceived) {
        confirmationReceived = processCommunication('O', 'o');
        normalOffState1 = true;
        normalOffState2 = true;
        ledFlag = true;
    } else {
        if (ledFlag){
            serialComm.write("x", 1);
            timer.reset();
            ledFlag = false;
        }
        if (elapsed_t_s(timer) > LED_ON_OFF_TIME) {
            normalOffState1 = false;
            normalOffState2 = false;
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
        startTime = elapsed_t_s(timer);
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
    } else if (elapsed_t_s(timer) > (startTime + TIME_FOR_OVERTIME)) {
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
        }
    } else {
        led1 = normalState1;
        led2 = normalState2;
    }
}
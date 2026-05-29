/**
 * @file    main.c
 * @brief   Ejemplo de integración: DS18B20 + UART en PIC18F4550.
 * @details Lee la temperatura cada 2 segundos y la envía por UART
 *          a 9600 baudios. Si supera el umbral de alarma (38 °C),
 *          activa un LED de alerta en RD0.
 *
 * @author  Jeison Tuquerrez
 * @date    2026
 * @version 1.0
 *
 * @note    Conexiones:
 *          - RC6  → TX (USB-Serial al PC)
 *          - RB0  → Data DS18B20 (pull-up 4.7 kΩ a VDD)
 *          - RD0  → LED de temperatura normal  (verde)
 *          - RD1  → LED de alarma temperatura  (rojo)
 */


// BITS DE CONFIGURACIÓN (FUSIBLES) - PIC18F4550 @ 8MHz interno
#pragma config FOSC   = INTOSCIO_EC  // Oscilador interno, RA6 como I/O
#pragma config PWRT   = ON           // Power-up Timer habilitado
#pragma config WDT    = OFF          // Watchdog deshabilitado
#pragma config PBADEN = OFF          // RB<4:0> como digitales al reset
#pragma config MCLRE  = ON           // MCLR habilitado
#pragma config LVP    = OFF          // Programación en baja tensión deshabilitada
#pragma config XINST  = OFF          // Conjunto de instrucciones extendido deshabilitado

#include <xc.h>
#include "uart.h"
#include "ds18b20.h"


// DEFINICIONES DE LEDS

#define LED_ENCENDIDO   LATDbits.LATD0  ///< Verde: sistema encendido
#define LED_FUNCIONAL   LATDbits.LATD1  ///< Azul:  leyendo sensores
#define LED_PREPARANDO  LATDbits.LATD2  ///< Amarillo: inicializando
#define LED_ALARMA      LATDbits.LATD3  ///< Rojo: temperatura fuera de rango

#define TEMP_ALARMA_MAX  38.0f          ///< Umbral superior de temperatura (°C)
#define TEMP_ALARMA_MIN  35.0f          ///< Umbral inferior de temperatura (°C)


// PROTOTIPOS PRIVADOS

static void Sistema_Init(void);
static void LED_Init(void);


// PROGRAMA PRINCIPAL


/**
 * @brief   Punto de entrada del programa.
 * @details Inicializa periféricos y entra al bucle principal donde
 *          lee temperatura y la transmite por UART cada 2 segundos.
 */
void main(void) {
    float temperatura = 0.0f;
    unsigned char resultado;

    /* 1. Inicialización general */
    Sistema_Init();
    LED_Init();

    LED_ENCENDIDO  = 1;   // Indica que el sistema arrancó
    LED_PREPARANDO = 1;   // Indica que está inicializando

    /* 2. Inicializa periféricos */
    UART_Init();
    DS18B20_Init();

    __delay_ms(100);      // Espera estabilización

    /* 3. Mensaje de arranque */
    UART_SendString("=====================================\r\n");
    UART_SendString("  Monitor de Temperatura - PIC18F4550\r\n");
    UART_SendString("  Sensor: DS18B20 | UART: 9600 baud  \r\n");
    UART_SendString("=====================================\r\n");

    LED_PREPARANDO = 0;   // Fin de inicialización
    LED_FUNCIONAL  = 1;   // Sistema funcionando

    /* 4. Bucle principal */
    while (1) {
        /* --- Inicia conversión de temperatura --- */
        resultado = DS18B20_StartConversion();  // Bloquea ~750 ms

        if (resultado == DS18B20_OK) {
            /* --- Lee el resultado --- */
            resultado = DS18B20_ReadTemperature(&temperatura);

            if (resultado == DS18B20_OK) {
                /* --- Envía por UART --- */
                UART_SendString("Temp: ");
                UART_SendFloat(temperatura, 2);
                UART_SendString(" C");

                /* --- Gestión de alarma --- */
                if (temperatura > TEMP_ALARMA_MAX) {
                    LED_ALARMA = 1;
                    UART_SendString(" [!] FIEBRE ALTA");
                } else if (temperatura < TEMP_ALARMA_MIN) {
                    LED_ALARMA = 1;
                    UART_SendString(" [!] HIPOTERMIA");
                } else {
                    LED_ALARMA = 0;
                    UART_SendString(" [OK]");
                }
                UART_SendString("\r\n");

            } else {
                UART_SendString("[ERROR] Fallo al leer scratchpad\r\n");
                LED_ALARMA = 1;
            }

        } else {
            UART_SendString("[ERROR] DS18B20 no detectado\r\n");
            LED_ALARMA = 1;
        }

        /* Espera 1.25 s adicionales → ciclo total ~2 s */
        __delay_ms(1250);
    }
}

// FUNCIÓN: CONFIGURACIÓN DEL SISTEMA

/**
 * @brief   Configura el oscilador interno y los pines como digitales.
 */
static void Sistema_Init(void) {
    OSCCON = 0x72;             // Oscilador interno a 8 MHz (IRCF = 111)
    ADCON1 = 0x0F;             // Todos los pines AN como digitales
    CMCON  = 0x07;             // Comparadores deshabilitados
}

// FUNCIÓN: INICIALIZAR LEDs


/**
 * @brief   Configura RD0–RD3 como salidas digitales para los LEDs de estado.
 */
static void LED_Init(void) {
    TRISDbits.TRISD0 = 0;   // LED Encendido  → Salida
    TRISDbits.TRISD1 = 0;   // LED Funcional  → Salida
    TRISDbits.TRISD2 = 0;   // LED Preparando → Salida
    TRISDbits.TRISD3 = 0;   // LED Alarma     → Salida

    LATD = 0x00;             // Apaga todos los LEDs al inicio
}

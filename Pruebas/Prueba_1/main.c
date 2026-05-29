/**
 * @file    main.c
 * @brief   Ejemplo de integraci�n: DS18B20 + UART en PIC18F4550.
 * @details Lee la temperatura cada 2 segundos y la env�a por UART
 *          a 9600 baudios. Si supera el umbral de alarma (38 �C),
 *          activa un LED de alerta en RD0.
 *
 * @author  Jeison Tuquerrez 
 * @date    2026
 * @version 1.0
 *
 * @note    Conexiones:
 *          - RC6  ? TX (USB-Serial al PC)
 *          - RB0  ? Data DS18B20 (pull-up 4.7 k? a VDD)
 *          - RD0  ? LED de temperatura normal  (verde)
 *          - RD1  ? LED de alarma temperatura  (rojo)
 */


// BITS DE CONFIGURACI�N (FUSIBLES) - PIC18F4550 @ 8MHz interno
#pragma config FOSC   = INTOSCIO_EC  // Oscilador interno, RA6 como I/O
#pragma config PWRT   = ON           // Power-up Timer habilitado
#pragma config WDT    = OFF          // Watchdog deshabilitado
#pragma config PBADEN = OFF          // RB<4:0> como digitales al reset
#pragma config MCLRE  = ON           // MCLR habilitado
#pragma config LVP    = OFF          // Programaci�n en baja tensi�n deshabilitada
#pragma config XINST  = OFF          // Conjunto de instrucciones extendido deshabilitado

#include <xc.h>
#include "uart.h"
#include "ds18b20.h"


// DEFINICIONES DE LEDS

#define LED_ENCENDIDO   LATDbits.LATD0  ///< Verde:    sistema encendido  (RD0)
#define LED_FUNCIONAL   LATDbits.LATD4  ///< Azul:     leyendo sensores   (RD4) ? RD1 ocupado por DS18B20
#define LED_PREPARANDO  LATDbits.LATD2  ///< Amarillo: inicializando       (RD2)
#define LED_ALARMA      LATDbits.LATD3  ///< Rojo:     temperatura fuera  (RD3)

#define TEMP_ALARMA_MAX  38.0f          ///< Umbral superior de temperatura (�C)
#define TEMP_ALARMA_MIN  35.0f          ///< Umbral inferior de temperatura (�C)


// PROTOTIPOS PRIVADOS

static void Sistema_Init(void);
static void LED_Init(void);

// PROGRAMA PRINCIPAL

/**
 * @brief   Punto de entrada del programa.
 * @details Inicializa perif�ricos y entra al bucle principal donde
 *          lee temperatura y la transmite por UART cada 2 segundos.
 */
void main(void) {
    float temperatura = 0.0f;
    unsigned char resultado;

    /* 1. Inicializaci�n general */
    Sistema_Init();
    LED_Init();

    LED_ENCENDIDO  = 1;   // Indica que el sistema arranc�
    LED_PREPARANDO = 1;   // Indica que est� inicializando

    /* 2. Inicializa perif�ricos */
    UART_Init();
    DS18B20_Init();

    __delay_ms(100);      // Espera estabilizaci�n

    /* 3. Mensaje de arranque */
    UART_SendString("=====================================\r\n");
    UART_SendString("  Monitor de Temperatura - PIC18F4550\r\n");
    UART_SendString("  Sensor: DS18B20 | UART: 9600 baud  \r\n");
    UART_SendString("=====================================\r\n");

    LED_PREPARANDO = 0;   // Fin de inicializaci�n
    LED_FUNCIONAL  = 1;   // Sistema funcionando

    /* 4. Bucle principal */
    while (1) {
        /* --- Paso 1: Reset y detecci�n de presencia --- */
        unsigned char reset_ok = DS18B20_Reset();
        if (reset_ok != DS18B20_OK) {
            UART_SendString("[ERROR] Sin pulso de presencia ? revisar:\r\n");
            UART_SendString("  1. Cable DATA conectado a RD1\r\n");
            UART_SendString("  2. Pull-up 4.7k entre VDD y DATA\r\n");
            UART_SendString("  3. VDD del sensor conectado a 5V\r\n");
            UART_SendString("  4. GND del sensor conectado a GND\r\n");
            LED_ALARMA = 1;
            __delay_ms(2000);
            continue;
        }
        UART_SendString("[OK] Sensor detectado\r\n");

        /* --- Paso 2: Conversi�n --- */
        resultado = DS18B20_StartConversion();
        if (resultado != DS18B20_OK) {
            UART_SendString("[ERROR] Fallo al iniciar conversion\r\n");
            LED_ALARMA = 1;
            __delay_ms(2000);
            continue;
        }

        /* --- Paso 3: Lectura --- */
        resultado = DS18B20_ReadTemperature(&temperatura);

        if (resultado == DS18B20_OK) {
            /* Detecta lectura inv�lida (0x0000 o 0xFFFF en el bus) */
            if (temperatura == 0.0f) {
                UART_SendString("[WARN] Temperatura = 0 ? posible corto en DATA o sin pull-up\r\n");
            } else if (temperatura == 85.0f) {
                UART_SendString("[WARN] Temperatura = 85C ? valor de reset, conversion incompleta\r\n");
            } else {
                /* Lectura v�lida */
                UART_SendString("Temp: ");
                UART_SendFloat(temperatura, 2);
                UART_SendString(" C");

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
            }
        } else {
            UART_SendString("[ERROR] Fallo al leer scratchpad\r\n");
            LED_ALARMA = 1;
        }

        __delay_ms(1250);
    }
}

// FUNCI�N: CONFIGURACI�N DEL SISTEMA

/**
 * @brief   Configura el oscilador interno y los pines como digitales.
 */
static void Sistema_Init(void) {
    OSCCON = 0x72;             // Oscilador interno a 8 MHz (IRCF = 111)
    ADCON1 = 0x0F;             // Todos los pines AN como digitales
    CMCON  = 0x07;             // Comparadores deshabilitados
}

// FUNCI�N: INICIALIZAR LEDs

/**
 * @brief   Configura RD0?RD3 como salidas digitales para los LEDs de estado.
 */
static void LED_Init(void) {
    TRISDbits.TRISD0 = 0;   // LED Encendido  ? Salida (RD0)
    // RD1 es el pin 1-Wire del DS18B20 ? lo gestiona la librer�a
    TRISDbits.TRISD2 = 0;   // LED Preparando ? Salida (RD2)
    TRISDbits.TRISD3 = 0;   // LED Alarma     ? Salida (RD3)
    TRISDbits.TRISD4 = 0;   // LED Funcional  ? Salida (RD4)

    LATD = 0x00;             // Apaga todos los LEDs al inicio
}
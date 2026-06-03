/**
 * @file    uart.h
 * @brief   Librería de comunicación UART (solo TX) para el PIC18F4550.
 * @details Configura el módulo USART del PIC18F4550 para transmisión
 * serie asíncrona a 9600 baudios utilizando el oscilador interno a 8 MHz.
 * Diseñada de forma ligera para telemetría sin depender de stdio.h.
 *
 * @author  Jeison Tuquerrez
 * @date    2026
 * @version 1.0
 *
 * @note    Conexión de Hardware:
 *          - RC6 → TX (Salida hacia PC / convertidor USB-Serial)
 *          - RC7 → RX (No utilizado, pero debe configurarse como entrada)
 * @note    Oscilador interno configurado a 8 MHz (OSCCON = 0x72)
 * @warning Esta librería implementa SOLO transmisión. No incluye funciones de recepción.
 */

#ifndef UART_H
#define UART_H

#include <xc.h>

/**
 * @defgroup UART_Config Configuración de Baudios
 * @brief Constantes para el cálculo automático de la velocidad de transmisión.
 * @{
 */

/** @brief Frecuencia del oscilador interno en Hz. */
#ifndef _XTAL_FREQ
#define _XTAL_FREQ      8000000UL
#endif

/** 
 * @brief Frecuencia efectiva para el cálculo del SPBRG.
 * @details En modo asíncrono de baja velocidad (BRGH=0), el timer se divide por 64.
 * @see Hoja de datos del PIC18F4550, sección USART.
 */
#define F_CPU_UART      (_XTAL_FREQ / 64)

/** @brief Velocidad de transmisión objetivo en baudios (bits por segundo). */
#define UART_BAUD_RATE  9600

/**
 * @brief Valor para el registro SPBRG.
 * @details Fórmula: SPBRG = (F_CPU / (64 * BAUD)) - 1 para modo de baja velocidad.
 * @note Para 8 MHz y 9600 baud: SPBRG = (8000000/(64*9600)) - 1 = 12.02 ≈ 12
 */
#define UART_SPBRG_VAL  ((unsigned int)((F_CPU_UART / UART_BAUD_RATE) - 1))

/** @} */ // Fin de UART_Config

/**
 * @defgroup UART_Registers Configuración de Registros
 * @brief Valores de inicialización para los registros TXSTA y RCSTA.
 * @{
 */

/** 
 * @brief Valor de inicialización para el registro TXSTA.
 * @details Bit a bit:
 *          - Bit 7 (CSRC) = 0 (Modo asíncrono, no aplica)
 *          - Bit 6 (TX9)  = 0 (Transmisión de 8 bits)
 *          - Bit 5 (TXEN) = 1 (Transmisor habilitado)
 *          - Bit 4 (SYNC) = 0 (Modo asíncrono)
 *          - Bit 2 (BRGH) = 0 (Baja velocidad, BRGH=0)
 * @note TXSTA = 0b00100000 = 0x20
 */
#define UART_TXSTA_VAL  0x20

/** 
 * @brief Valor de inicialización para el registro RCSTA.
 * @details Bit a bit:
 *          - Bit 7 (SPEN) = 1 (Puerto serie habilitado)
 *          - Bit 4 (CREN) = 1 (Recepción continua, necesaria para estabilidad)
 * @note RCSTA = 0b10010000 = 0x90
 */
#define UART_RCSTA_VAL  0x90

/** @} */ // Fin de UART_Registers

/**
 * @defgroup UART_Functions Funciones de Transmisión UART
 * @brief API completa para transmisión de datos por puerto serie.
 * @{
 */

/** 
 * @brief Inicializa el módulo USART del PIC18F4550.
 * @details Configura los pines RC6/TX como salida y RC7/RX como entrada,
 *          establece la velocidad a 9600 baudios y habilita el módulo.
 * @pre El oscilador debe estar configurado a 8 MHz.
 * @post El módulo USART está listo para transmitir datos.
 */
void UART_Init(void);

/** 
 * @brief Envía un único carácter por el bus serie.
 * @param c Carácter ASCII a transmitir (8 bits).
 * @details Función bloqueante: Espera a que el buffer de transmisión esté vacío
 *          antes de cargar el nuevo dato en TXREG.
 * @note TXIF se pone a 1 cuando el buffer está vacío. En modo sin interrupciones,
 *       esta bandera debe ser consultada antes de cada envío.
 */
void UART_SendChar(char c);

/** 
 * @brief Envía una cadena de texto completa.
 * @param str Puntero a cadena terminada en carácter nulo ('\0').
 * @details Itera sobre la cadena enviando cada carácter mediante UART_SendChar().
 */
void UART_SendString(const char *str);

/** 
 * @brief Convierte un entero sin signo a texto y lo transmite.
 * @param val Número a transmitir (rango: 0 a 65535).
 * @details Utiliza división sucesiva por 10 para extraer dígitos,
 *          evitando el uso de sprintf() para ahorrar memoria.
 * @warning El parámetro es unsigned int de 16 bits. Para valores mayores,
 *          modificar el buffer (actualmente de 6 bytes).
 */
void UART_SendUInt(unsigned int val);

/** 
 * @brief Convierte un número flotante a texto y lo transmite.
 * @param valor Número a transmitir.
 * @param decimals Cantidad de decimales a mostrar (recomendado: 1 a 4).
 * @details Maneja signo negativo, separa parte entera y fraccionaria,
 *          y aplica redondeo al último decimal.
 * @warning La parte entera se almacena en unsigned int (16 bits, máximo 65535).
 *          Valores mayores producirán desbordamiento. Para números más grandes,
 *          cambiar integer_part a unsigned long.
 * @note Para mostrar "0.05" con decimals=2, añade el cero a la izquierda automáticamente.
 */
void UART_SendFloat(float valor, unsigned char decimals);

/** @} */ // Fin de UART_Functions

#endif /* UART_H */
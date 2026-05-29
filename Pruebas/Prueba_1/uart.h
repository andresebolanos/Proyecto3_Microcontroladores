/**
 * @file    uart.h
 * @brief   Librería de comunicación UART (solo TX) para PIC18F4550.
 * @details Configura el módulo USART del PIC18F4550 para transmisión
 *          serie asíncrona a 9600 baudios con oscilador interno a 8 MHz.
 *          Solo incluye funciones de transmisión (TX).
 *
 * @author  Tu Nombre
 * @date    2026
 * @version 1.0
 *
 * @note    Conexión hardware:
 *          - RC6 → TX (Salida hacia PC / convertidor USB-Serial)
 *          - Oscilador interno: 8 MHz (OSCCON = 0x72)
 */

#ifndef UART_H
#define UART_H

#include <xc.h>

// =========================================================
// CONSTANTES DE CONFIGURACIÓN
// =========================================================

/** @brief Frecuencia del oscilador interno en Hz */
#define _XTAL_FREQ      8000000UL

/** @brief Frecuencia efectiva tras prescaler de 64 para SPBRG */
#define F_CPU_UART      (_XTAL_FREQ / 64)

/** @brief Baudrate objetivo */
#define UART_BAUD_RATE  9600

/**
 * @brief Macro para calcular el valor del registro SPBRG.
 * @details Fórmula: SPBRG = (F_CPU / baud) - 1  (modo baja velocidad, BRGH=0)
 */
#define UART_SPBRG_VAL  ((unsigned int)((F_CPU_UART / UART_BAUD_RATE) - 1))

// =========================================================
// PROTOTIPOS DE FUNCIONES PÚBLICAS
// =========================================================

/**
 * @brief   Inicializa el módulo USART en modo transmisión asíncrona.
 * @details Configura RC6 como salida TX, establece 9600 baudios,
 *          habilita el transmisor y enciende el módulo serial.
 *          Debe llamarse una sola vez al inicio del programa.
 */
void UART_Init(void);

/**
 * @brief   Transmite un único carácter por UART.
 * @details Espera a que el registro de transmisión esté libre (TXIF=1)
 *          antes de cargar el carácter en TXREG.
 * @param   c   Carácter ASCII a transmitir.
 */
void UART_SendChar(char c);

/**
 * @brief   Transmite una cadena de caracteres terminada en '\0'.
 * @details Llama internamente a UART_SendChar() para cada carácter.
 * @param   str Puntero a la cadena de texto a enviar.
 */
void UART_SendString(const char *str);

/**
 * @brief   Transmite un número entero sin signo en formato decimal.
 * @details Convierte el valor a texto y lo envía carácter por carácter.
 *          No añade '\n' ni '\r' automáticamente.
 * @param   val Valor entero sin signo a mostrar (0 – 65535).
 */
void UART_SendUInt(unsigned int val);

/**
 * @brief   Transmite un número de punto flotante con 2 decimales.
 * @details Separa parte entera y decimal para evitar dependencia de
 *          funciones printf() pesadas.
 * @param   val     Valor flotante a enviar.
 * @param   decimals Cantidad de cifras decimales a mostrar (máx. 4).
 */
void UART_SendFloat(float val, unsigned char decimals);

#endif /* UART_H */

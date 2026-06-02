/**
 * @file    uart.c
 * @brief   Implementación de la librería UART (solo TX) para PIC18F4550.
 * @details Módulo USART configurado en modo asíncrono a 9600 baudios,
 *          baja velocidad (BRGH=0), 8 bits de datos, sin paridad, 1 bit de stop.
 *
 * @author  Jeison Tuquerrez
 * @date    2026
 * @version 1.0
 */

#include "uart.h"

// FUNCI0N: INICIALIZAR UART

/**
 * @brief   Inicializa el módulo USART del PIC18F4550.
 * @details Pasos de configuración:
 *          1. RC6 como salida (TX), RC7 como entrada (RX, no usada).
 *          2. Carga SPBRG con el valor calculado para 9600 baudios.
 *          3. TXSTA: habilita transmisor, modo asíncrono, baja velocidad.
 *          4. RCSTA: enciende el módulo USART.
 */
void UART_Init(void) {
    /* --- Dirección de pines --- */
    TRISCbits.TRISC6 = 0;   // RC6/TX → Salida
    TRISCbits.TRISC7 = 1;   // RC7/RX → Entrada (no usada, pero requerida por hardware)

    /* --- Velocidad de baudios --- */
    SPBRG = UART_SPBRG_VAL; // 9600 baud @ 8 MHz → SPBRG = 12

    /* --- Registro TXSTA ---
     *   Bit 7 (CSRC) = 0  Modo asincrono
     *   Bit 6 (TX9)  = 0  8 bits de datos
     *   Bit 5 (TXEN) = 1  Transmisor habilitado
     *   Bit 4 (SYNC) = 0  Modo asincrono
     *   Bit 2 (BRGH) = 0  Baja velocidad
     */
    TXSTA = 0x20;

    /* --- Registro RCSTA ---
     *   Bit 7 (SPEN) = 1  → Puerto serial habilitado (RC6/RC7 como UART)
     *   Bit 4 (CREN) = 1  → Recepción continua (requerida para estabilidad)
     */
    RCSTA = 0x90;
}

// FUNCIÓN: TRANSMITIR UN CARÁCTER

/**
 * @brief   Envia un caracter por el puerto serial.
 * @param   c  Byte a transmitir.
 */
void UART_SendChar(char c) {
    while (PIR1bits.TXIF == 0);  // Espera a que el buffer TX etsa vacio 
    TXREG = c;                   // Carga el caracter y  dispara la transmision
}

// FUNCIÓN: TRANSMITIR CADENA DE TEXTO

/**
 * @brief   Envía una cadena terminada en '\0' carácter por carácter.
 * @param   str  Puntero a la cadena a transmitir.
 */
void UART_SendString(const char *str) {
    while (*str != '\0') {
        UART_SendChar(*str);
        str++;
    }
}

// FUNCIÓN: TRANSMITIR ENTERO SIN SIGNO

/**
 * @brief   Convierte un entero sin signo a ASCII y lo transmite.
 * @details Usa división sucesiva para extraer cada digito decimal.
 *          Caso especial: si val == 0, envia directamente '0'.
 * @param   val  numero o a transmitir (0  65535).
 */
void UART_SendUInt(unsigned int val) {
    char buf[6];          // Max 5 digitos + terminador
    unsigned char i = 0;

    if (val == 0) {
        UART_SendChar('0');
        return;
    }

    /* Extrae dígitos en orden inverso */
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }

    /* Envía en orden correcto (de mayor a menor peso) */
    while (i > 0) {
        UART_SendChar(buf[--i]);
    }
}

// FUNCIÓN: TRANSMITIR FLOTANTE

/**
 * @brief   Envia un numero flotante con la cantidad de decimales indicada.
 * @details Maneja valores negativos. Separa parte entera y decimal
 *          sin usar printf() para mantener bajo el uso de memoria.
 * @param   val      Numero flotante a enviar.
 * @param   decimals Cifras decimales deseadas (1 a 4).
 */
void UART_SendFloat(float val, unsigned char decimals) {
    unsigned int integer_part;
    unsigned int decimal_part;
    unsigned char d;
    float multiplier = 1.0f;

    /* Manejo de negativos */
    if (val < 0.0f) {
        UART_SendChar('-');
        val = -val;
    }

    /* Calcula multiplicador segun decimales pedidos */
    for (d = 0; d < decimals; d++) {
        multiplier *= 10.0f;
    }

    integer_part = (unsigned int)val;
    decimal_part = (unsigned int)((val - (float)integer_part) * multiplier + 0.5f);

    UART_SendUInt(integer_part);
    UART_SendChar('.');
    
    /* Relleno con ceros a la izquierda si la parte decimal es pequena */
    unsigned int temp = decimal_part;
    for (d = 1; d < decimals; d++) {
        multiplier /= 10.0f;
        if (temp < (unsigned int)multiplier) {
            UART_SendChar('0');
        }
    }
    UART_SendUInt(decimal_part);
}

/**
 * @file    uart.c
 * @brief   Implementación de las funciones de transmisión serie UART.
 * @details Manipula los registros TXSTA, RCSTA y SPBRG para inicializar
 *          el hardware de comunicación y provee rutinas de bajo nivel para 
 *          el envío de caracteres, cadenas y conversiones numéricas.
 * 
 * @author  Jeison Tuquerrez
 * @date    2026
 * @version 1.0
 */

#include "uart.h"

/* =========================================
 * FUNCIONES DE INICIALIZACIÓN
 * ========================================= */

/**
 * @defgroup UART_InitImpl Inicialización del Hardware
 * @brief Configuración de pines y registros del USART.
 * @{
 */

/**
 * @brief   Inicializa el módulo USART del PIC18F4550.
 * @details Pasos de configuración hardware:
 *          1. TRISC: RC6 como salida (TX), RC7 como entrada (RX).
 *          2. SPBRG: Se carga con el valor precalculado para 9600 baudios.
 *          3. TXSTA: Se habilita el transmisor en modo asíncrono y baja velocidad.
 *          4. RCSTA: Se enciende el módulo USART y se habilita recepción continua.
 * @post    El módulo USART está listo para transmitir a 9600 baudios, 8 bits, sin paridad.
 */
void UART_Init(void) {
    /* --- Configuración de pines --- */
    TRISCbits.TRISC6 = 0;   /* RC6/TX → Salida */
    TRISCbits.TRISC7 = 1;   /* RC7/RX → Entrada (requerida por hardware aunque no se use) */

    /* --- Velocidad de baudios --- */
    SPBRG = UART_SPBRG_VAL; /* 9600 baud @ 8 MHz → SPBRG = 12 */

    /* --- Registro TXSTA: Configuración del transmisor --- */
    /* Bit 7 (CSRC) = 0  (Modo asíncrono, este bit no se usa)
     * Bit 6 (TX9)  = 0  (Transmisión de 8 bits, no 9)
     * Bit 5 (TXEN) = 1  (Transmisor habilitado)
     * Bit 4 (SYNC) = 0  (Modo asíncrono)
     * Bit 2 (BRGH) = 0  (Baja velocidad, divisor por 64)
     */
    TXSTA = UART_TXSTA_VAL;

    /* --- Registro RCSTA: Configuración del receptor --- */
    /* Bit 7 (SPEN) = 1  (Puerto serial habilitado: RC6/RC7 mapeados a USART)
     * Bit 4 (CREN) = 1  (Recepción continua - necesaria para estabilidad del módulo)
     */
    RCSTA = UART_RCSTA_VAL;
}

/** @} */ // Fin de UART_InitImpl

/* =========================================
 * FUNCIONES DE TRANSMISIÓN
 * ========================================= */

/**
 * @defgroup UART_TxImpl Funciones de Transmisión
 * @brief Implementación de envío de datos por UART.
 * @{
 */

/**
 * @brief   Envía un único carácter por el bus serie.
 * @param   c Byte o carácter ASCII a transmitir.
 * @details Función bloqueante: Monitorea la bandera TXIF del registro PIR1 
 *          y espera a que el buffer del transmisor esté vacío antes de cargar 
 *          el nuevo dato en TXREG.
 * @note    TXIF se pone a 1 cuando el registro TXREG está vacío y listo para 
 *          recibir un nuevo dato. En modo sin interrupciones, este polling es suficiente.
 */
void UART_SendChar(char c) {
    while (!PIR1bits.TXIF);  /* Espera a que el buffer TX esté vacío (TXIF = 1) */ 
    TXREG = c;               /* Carga el carácter y dispara físicamente la transmisión */
}

/**
 * @brief   Envía una cadena de texto (String) completa.
 * @param   str Puntero a la cadena de caracteres terminada en carácter nulo ('\0').
 * @details Itera sobre el puntero char enviando byte por byte mediante UART_SendChar().
 */
void UART_SendString(const char *str) {
    while (*str != '\0') {
        UART_SendChar(*str);
        str++;
    }
}

/**
 * @brief   Convierte un número entero sin signo a texto ASCII y lo transmite.
 * @param   val Valor numérico entero a transmitir (Rango: 0 a 65535).
 * @details Emplea el método de división sucesiva (módulo 10) para extraer los dígitos
 *          evitando el uso de la pesada librería sprintf(). Se envían en orden de mayor a menor peso.
 * @warning El buffer local tiene capacidad para 6 bytes (5 dígitos + terminador).
 *          Si se modifica el tipo a unsigned long (10 dígitos), debe aumentarse.
 */
void UART_SendUInt(unsigned int val) {

    char buf[6];      /**< Buffer temporal para almacenar dígitos en orden inverso */
    unsigned char i = 0; /**< Índice del buffer y contador de dígitos */

    /* Caso especial: valor cero */
    if (val == 0) {
        UART_SendChar('0');
        return;
    }

    /* Extrae los dígitos desde la unidad hacia arriba (quedan en orden inverso) */
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }

    /* Recorre el buffer al revés para enviarlos en el orden numérico correcto */
    while (i > 0) {
        UART_SendChar(buf[--i]);
    }
}

/**
 * @brief   Convierte un número de punto flotante a texto ASCII y lo envía.
 * @param   valor Valor flotante a transmitir (Ej: 36.52).
 * @param   decimals Cantidad de cifras decimales que se desean mostrar (1 a 4 máximo recomendado).
 * @details Aborda los valores negativos, separa la parte entera de la parte fraccionaria 
 *          e imprime ambas mitades divididas por un punto decimal. Esto ahorra valiosa memoria ROM y RAM.
 * @warning La parte entera se almacena en unsigned int (16 bits, máximo 65535).
 *          Por ejemplo, 100000.5 producirá desbordamiento. Para números mayores,
 *          cambiar integer_part a unsigned long.
 * @note    El redondeo se aplica en el último decimal usando +0.5f antes de truncar.
 * @note    Para decimals=2, 3.1416 → "3.14" (truncado, no redondeado hacia arriba).
 */
void UART_SendFloat(float valor, unsigned char decimals) {

    unsigned int integer_part;   /**< Parte entera del número */
    unsigned int decimal_part;   /**< Parte decimal escalada y redondeada */
    unsigned char d;             /**< Contador de decimales */
    float multiplier = 1.0f;     /**< Multiplicador 10^decimals */
    unsigned int temp;           /**< Variable temporal para relleno de ceros */

    /* Manejo del signo negativo */
    if (valor < 0.0f) {
        UART_SendChar('-');
        valor = -valor;
    }

    /* Cálculo de la potencia base-10 según la cantidad de decimales */
    for (d = 0; d < decimals; d++) {
        multiplier *= 10.0f;
    }

    /* Extracción de partes con redondeo */
    integer_part = (unsigned int)valor;
    decimal_part = (unsigned int)((valor - (float)integer_part) * multiplier + 0.5f);

    /* Envío de la parte entera y el separador decimal */
    UART_SendUInt(integer_part);
    UART_SendChar('.');
    
    /* 
     * Relleno con ceros a la izquierda si la parte decimal inicia en cero.
     * Ejemplo: valor=0.05, decimals=2 → decimal_part=5, debe mostrar "0.05" no "0.5"
     */
    temp = decimal_part;
    for (d = 1; d < decimals; d++) {
        multiplier /= 10.0f;
        if (temp < (unsigned int)multiplier) {
            UART_SendChar('0');
        }
    }
    UART_SendUInt(decimal_part);
}

/** @} */ // Fin de UART_TxImpl
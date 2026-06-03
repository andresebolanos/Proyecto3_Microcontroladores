/**
 * @file    ds18b20.c
 * @brief   Implementación del protocolo 1-Wire para el sensor DS18B20.
 * @details Los tiempos del protocolo 1-Wire están estrictamente calibrados para
 *          el oscilador interno del PIC18F4550 a 8 MHz utilizando el compilador XC8.
 *
 *          Tiempos estándar del protocolo 1-Wire (Maxim AN 126):
 *          - Reset pulse:     480 µs mínimo.
 *          - Presence pulse:  60 a 240 µs (generado por el sensor).
 *          - Write '0' slot:  60 a 120 µs en LOW.
 *          - Write '1' slot:  1 a 15 µs en LOW, luego HIGH hasta completar 60 µs.
 *          - Read slot:       1 µs en LOW, leer el pin antes de los 15 µs.
 * 
 * @author  Jeison Tuquerrez
 * @date    2026
 * @version 1.0
 */

#include "ds18b20.h"

/* =========================================================
 * FUNCIONES PRIVADAS DE CONTROL DE BUS 1-WIRE (BIT-BANGING)
 * ========================================================= */

/**
 * @defgroup OW_Private Primitivas Privadas del Bus 1-Wire
 * @brief Funciones de bajo nivel para comunicación bit a bit.
 * @{
 */

/**
 * @brief   Transmite un único bit lógico a través del bus 1-Wire.
 * @param   bit Valor del bit a enviar (0 o 1).
 * @details Tiempos de escritura:
 *          - Write '1': La línea baja por 6 µs y luego se libera por 64 µs.
 *          - Write '0': La línea baja por 60 µs y luego se libera por 10 µs.
 * @note    Los tiempos están ligeramente redondeados respecto al estándar
 *          (70 µs total para '1' vs 60 µs teóricos) pero son totalmente
 *          compatibles gracias a la tolerancia del protocolo.
 */
static void OW_WriteBit(unsigned char bit) {
    if (bit) {
        /* Escribir '1': pulso corto en LOW */
        OW_LOW();
        __delay_us(OW_WRITE_1_LOW_US);
        OW_RELEASE();
        __delay_us(OW_WRITE_1_HIGH_US);
    } else {
        /* Escribir '0': pulso largo en LOW */
        OW_LOW();
        __delay_us(OW_WRITE_0_LOW_US);
        OW_RELEASE();
        __delay_us(OW_WRITE_0_HIGH_US);
    }
}

/**
 * @brief   Lee un único bit lógico desde el bus 1-Wire.
 * @details El máster baja la línea de 1 a 6 µs para iniciar el slot de lectura ("Read Time Slot").
 *          Luego libera el bus y captura el estado antes de los 15 µs (el sensor
 *          mantiene LOW si desea enviar un '0', o libera el bus si envía un '1').
 * @return  Estado lógico del bit recibido (0 o 1).
 */
static unsigned char OW_ReadBit(void) {

    unsigned char bit_val; /**< Bit leído desde el bus 1-Wire */
    
    OW_LOW();
    __delay_us(OW_READ_LOW_US);
    OW_RELEASE();
    __delay_us(OW_READ_SAMPLE_US);
    
    bit_val = OW_READ();
    
    __delay_us(OW_READ_RECOVER_US);
    return bit_val;
}

/**
 * @brief   Transmite un byte completo (8 bits) por el bus 1-Wire.
 * @param   byte Dato de 8 bits que se desea enviar.
 * @details La transmisión se realiza comenzando siempre por el LSB (Bit Menos Significativo).
 */
static void OW_WriteByte(unsigned char byte) {
    unsigned char i;
    for (i = 0; i < 8; i++) {
        OW_WriteBit(byte & 0x01);   /* Envía el LSB primero */
        byte >>= 1;                 /* Desplaza para el siguiente bit */
    }
}

/**
 * @brief   Recibe un byte completo (8 bits) desde el bus 1-Wire.
 * @details La recepción se realiza ensamblando los bits empezando por el LSB.
 * @return  El byte completo recibido desde el sensor.
 */
static unsigned char OW_ReadByte(void) {

    unsigned char i;          /**< Contador de bits */
    unsigned char byte = 0;   /**< Byte reconstruido desde el bus */
    
    for (i = 0; i < 8; i++) {
        if (OW_ReadBit()) {
            byte |= (1 << i);       /* Bit i-ésimo (LSB primero) */
        }
    }
    return byte;
}

/** @} */ // Fin de OW_Private

/* =========================================================
 * FUNCIONES PÚBLICAS DE LA LIBRERÍA
 * ========================================================= */

/**
 * @defgroup DS18B20_Impl Implementación de Funciones Públicas
 * @brief API completa para el control del sensor DS18B20.
 * @{
 */

/**
 * @brief   Inicializa físicamente el pin asignado al bus 1-Wire.
 * @details Configura el pin (RD1) como entrada. La resistencia externa de pull-up
 *          se encargará de mantener la línea en estado lógico ALTO (reposo).
 */
void DS18B20_Init(void) {
    OW_RELEASE();  /* Pin en alta impedancia, pull-up mantiene HIGH */
}

/**
 * @brief   Ejecuta la secuencia de Inicialización/Reset del protocolo 1-Wire.
 * @details Pasos de hardware:
 *          1. Máster baja la línea mínimo 480 µs (Se usan 500 µs por seguridad).
 *          2. Máster libera el bus.
 *          3. Se esperan 70 µs para permitir que el sensor reaccione.
 *          4. Máster lee la línea: Un estado BAJO indica pulso de presencia válido.
 *          5. Se espera a que finalice la ventana de tiempo del reset (~410 µs restantes).
 * @return  DS18B20_OK si el sensor respondió, o DS18B20_ERR_NO_DEVICE en caso contrario.
 */
unsigned char DS18B20_Reset(void) {

    unsigned char presence; /**< Estado del pulso de presencia */

    /* 1. Máster baja la línea */
    OW_LOW();
    __delay_us(OW_RESET_LOW_US);

    /* 2. Libera el bus */
    OW_RELEASE();
    __delay_us(OW_RESET_RECOVER_US);

    /* 3. Lee la respuesta del sensor (0 = presencia detectada) */
    presence = OW_READ();

    /* 4. Espera a que termine la ventana de reset */
    __delay_us(OW_RESET_WAIT_US);

    return (presence == 0) ? DS18B20_OK : DS18B20_ERR_NO_DEVICE;
}

/**
 * @brief   Envía los comandos necesarios para iniciar una conversión térmica.
 * @details Secuencia I/O: [RESET] -> [SKIP ROM (0xCC)] -> [CONVERT T (0x44)].
 *          Una vez enviados los comandos, la función bloquea el flujo del programa
 *          durante 750 ms para asegurar que la conversión a 12 bits haya finalizado.
 * @warning Esta función es BLOQUEANTE: detiene la ejecución por 750 ms.
 *          En sistemas con requisitos de tiempo real, considere usar temporizadores.
 * @return  DS18B20_OK si el comando se envió con éxito, 
 *          DS18B20_ERR_NO_DEVICE si no hay sensor presente.
 */
unsigned char DS18B20_StartConversion(void) {

    unsigned char status; /**< Resultado de la detección del sensor */
    unsigned int i;       /**< Contador para el retardo de conversión */

    /* Reset y verificación de presencia */
    status = DS18B20_Reset();
    if (status != DS18B20_OK) {
        return DS18B20_ERR_NO_DEVICE;
    }

    /* Envío de comandos */
    OW_WriteByte(DS18B20_CMD_SKIP_ROM);     /* Omite búsqueda de ROM (único sensor) */
    OW_WriteByte(DS18B20_CMD_CONVERT_T);    /* Inicia conversión */

    /* 
     * Retardo de 750 ms (tiempo de conversión hardware máximo a 12 bits)
     * El sensor consume ~1.5 mA durante este período.
     */
    for (i = 0; i < DS18B20_CONV_TIME_MS; i++) {
        __delay_ms(1);
    }

    return DS18B20_OK;
}

/**
 * @brief   Lee la memoria del sensor y calcula la temperatura en grados Celsius (°C).
 * @param   temperature Puntero a la variable float donde se guardará la temperatura final.
 * @details Secuencia I/O: [RESET] -> [SKIP ROM (0xCC)] -> [READ SCRATCHPAD (0xBE)].
 *          El PIC extrae el LSB y el MSB. Al combinarlos se obtiene un entero de 16 bits
 *          con signo (resolución de 0.0625 °C por unidad LSB).
 * @note    Esta función NO verifica el CRC de los datos leídos.
 *          Para aplicaciones que requieren alta integridad, implemente la verificación
 *          usando el octavo byte del scratchpad.
 * @return  DS18B20_OK si la lectura fue exitosa, 
 *          DS18B20_ERR_NO_DEVICE si hubo fallo físico.
 */
unsigned char DS18B20_ReadTemperature(float *temperature) {
    
    unsigned char status;     /**< Resultado de la comunicación con el sensor */
    unsigned char lsb;        /**< Byte menos significativo de temperatura */
    unsigned char msb;        /**< Byte más significativo de temperatura */
    unsigned int raw;         /**< Valor crudo de 16 bits */
    signed int raw_signed;    /**< Valor crudo interpretado con signo */

    /* Reset y verificación de presencia */
    status = DS18B20_Reset();
    if (status != DS18B20_OK) {
        return DS18B20_ERR_NO_DEVICE;
    }

    /* Envío de comandos para lectura del scratchpad */
    OW_WriteByte(DS18B20_CMD_SKIP_ROM);           /* Omite búsqueda de ROM */
    OW_WriteByte(DS18B20_CMD_READ_SCRATCHPAD);    /* Solicita lectura de memoria */

    /* Lectura de los dos primeros bytes (temperatura) */
    lsb = OW_ReadByte();   /* Byte bajo (Parte entera + fracción baja) */
    msb = OW_ReadByte();   /* Byte alto (Signo + parte entera alta) */

    /* NOTA: El scratchpad tiene 9 bytes totales:
     * Byte 0: Temperatura LSB
     * Byte 1: Temperatura MSB
     * Byte 2: Registro TH
     * Byte 3: Registro TL
     * Byte 4: Registro de Configuración
     * Bytes 5-7: Reservados
     * Byte 8: CRC
     * Los bytes restantes no se leen en esta implementación.
     */

    /* Conversión a entero de 16 bits con signo */
    raw = ((unsigned int)msb << 8) | lsb;
    raw_signed = (signed int)raw;

    /*
     * Conversión final a grados Celsius:
     * - Resolución de 12 bits: 1 LSB = 0.0625°C
     * - Fórmula: temperatura = raw_signed * 0.0625f
     * - Ejemplo: raw = 0x0191 (401 decimal) -> 401 * 0.0625 = 25.0625°C
     */
    *temperature = (float)raw_signed * 0.0625f;

    return DS18B20_OK;
}

/** @} */ // Fin de DS18B20_Impl
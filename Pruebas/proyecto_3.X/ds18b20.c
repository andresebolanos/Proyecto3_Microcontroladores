/**
 * @file    ds18b20.c
 * @brief   Implementación del protocolo 1-Wire para el sensor DS18B20.
 * @details Los tiempos del protocolo 1-Wire están calibrados para
 *          el oscilador interno del PIC18F4550 a 8 MHz con XC8.
 *
 *          Tiempos estándar del protocolo 1-Wire (Maxim AN 126):
 *          - Reset pulse:     480 µs mínimo
 *          - Presence pulse:  60?240 µs (generado por el sensor)
 *          - Write '0' slot:  60?120 µs en LOW
 *          - Write '1' slot:  1?15 µs en LOW, luego HIGH hasta completar 60 µs
 *          - Read slot:       1 µs en LOW, leer en los primeros 15 µs
 *
 * @author  Jeison Tuquerrez
 * @date    2026
 * @version 1.0
 */

#include "ds18b20.h"

/* _XTAL_FREQ viene definido en ds18b20.h ? requerido por __delay_ms */

// FUNCIÓN PRIVADA: DELAY EN MICROSEGUNDOS
/**
 * @brief   Genera un retardo en microsegundos.
 * @details A 8 MHz, cada ciclo de instrucción = 0.5 µs (4 ciclos de reloj).
 *          El bucle está ajustado empíricamente para XC8 sin optimización.
 *          Para mayor precisión, se usa __delay_us() de xc.h (requiere _XTAL_FREQ).
 * @param   us  Microsegundos a esperar.
 */
/* __delay_us() se usa directamente ? más preciso que bucles con __nop() */

// FUNCIÓN PRIVADA: ESCRIBIR UN BIT

/**
 * @brief   Transmite un bit por el bus 1-Wire.
 * @details Write '1': LOW por 6 µs ? HIGH por 64 µs.
 *          Write '0': LOW por 60 µs ? HIGH por 10 µs.
 * @param   bit  Valor del bit a enviar (0 o 1).
 */
static void OW_WriteBit(unsigned char bit) {
    if (bit) {
        OW_LOW();
        __delay_us(6);
        OW_RELEASE();
        __delay_us(64);
    } else {
        OW_LOW();
        __delay_us(60);
        OW_RELEASE();
        __delay_us(10);
    }
}

// FUNCIÓN PRIVADA: LEER UN BIT

/**
 * @brief   Lee un bit del bus 1-Wire.
 * @details El master baja la línea 1?6 µs para iniciar el slot de lectura.
 *          Luego libera y lee el estado antes de los 15 µs (el sensor
 *          mantiene LOW si envía '0', o libera si envía '1').
 * @return  0 o 1 según el bit recibido.
 */
static unsigned char OW_ReadBit(void) {
    unsigned char bit_val;
    OW_LOW();
    __delay_us(6);
    OW_RELEASE();
    __delay_us(9);
    bit_val = OW_READ();
    __delay_us(55);
    return bit_val;
}

// FUNCIÓN PRIVADA: ESCRIBIR UN BYTE

/**
 * @brief   Transmite un byte completo por el bus 1-Wire (LSB primero).
 * @param   byte  Dato de 8 bits a enviar.
 */
static void OW_WriteByte(unsigned char byte) {
    unsigned char i;
    for (i = 0; i < 8; i++) {
        OW_WriteBit(byte & 0x01);  // Envía el bit menos significativo
        byte >>= 1;                // Desplaza al siguiente bit
    }
}

// FUNCIÓN PRIVADA: LEER UN BYTE

/**
 * @brief   Recibe un byte completo del bus 1-Wire (LSB primero).
 * @return  Byte de 8 bits recibido del sensor.
 */
static unsigned char OW_ReadByte(void) {
    unsigned char i;
    unsigned char byte = 0;
    for (i = 0; i < 8; i++) {
        if (OW_ReadBit()) {
            byte |= (1 << i);      // Coloca el bit en su posición
        }
    }
    return byte;
}

// FUNCIÓN PÚBLICA: INICIALIZAR

/**
 * @brief   Inicializa el pin RB0 como entrada (bus 1-Wire en reposo = HIGH).
 */
void DS18B20_Init(void) {
    OW_RELEASE();  // RB0 como entrada, pull-up externo mantiene HIGH
}

// FUNCIÓN PÚBLICA: RESET + DETECCIÓN DE PRESENCIA

/**
 * @brief   Ejecuta la secuencia de reset 1-Wire.
 * @return  DS18B20_OK si el sensor respondió, DS18B20_ERR_NO_DEVICE si no.
 */
unsigned char DS18B20_Reset(void) {
    unsigned char presence;

    /* 1. Master baja la línea mínimo 480 µs (usamos 500 µs para más margen) */
    OW_LOW();
    __delay_us(500);

    /* 2. Master libera ? el sensor debe bajar entre 15 y 60 µs después */
    OW_RELEASE();
    __delay_us(70);         // Espera 70 µs ? el sensor ya debió responder

    /* 3. Lee presencia: LOW = sensor respondió */
    presence = OW_READ();

    /* 4. Espera el resto del período de reset (960 µs total mínimo) */
    __delay_us(410);

    return (presence == 0) ? DS18B20_OK : DS18B20_ERR_NO_DEVICE;
}

// FUNCIÓN PÚBLICA: INICIAR CONVERSIÓN

/**
 * @brief   Envía los comandos para iniciar la conversión de temperatura.
 * @details Secuencia: RESET ? SKIP ROM (0xCC) ? CONVERT T (0x44).
 *          Luego bloquea 750 ms para que el sensor complete la conversión
 *          a 12 bits de resolución.
 * @return  DS18B20_OK o DS18B20_ERR_NO_DEVICE.
 */
unsigned char DS18B20_StartConversion(void) {
    unsigned char status;
    unsigned int i;

    status = DS18B20_Reset();
    if (status != DS18B20_OK) {
        return DS18B20_ERR_NO_DEVICE;
    }

    OW_WriteByte(DS18B20_CMD_SKIP_ROM);    // 0xCC: solo hay un sensor en el bus
    OW_WriteByte(DS18B20_CMD_CONVERT_T);   // 0x44: inicia conversión

    /* Espera 750 ms (tiempo de conversión máximo a 12 bits) */
    for (i = 0; i < 750; i++) {
        __delay_ms(1);
    }

    return DS18B20_OK;
}

// FUNCIÓN PÚBLICA: LEER TEMPERATURA

/**
 * @brief   Lee el resultado de la conversión y calcula la temperatura en °C.
 * @details Secuencia: RESET ? SKIP ROM (0xCC) ? READ SCRATCHPAD (0xBE).
 *          Lee los primeros 2 bytes del scratchpad:
 *          - Byte 0 (LSB): bits [7:0] del registro de temperatura.
 *          - Byte 1 (MSB): bits [15:8], incluye signo.
 *
 *          Conversión (12 bits, resolución 0.0625 °C):
 *          raw = (MSB << 8) | LSB
 *          Si bit 11 = 1 (negativo) ? complemento a 2.
 *          temperatura = raw_signed × 0.0625
 *
 * @param   temperature  Puntero donde se almacena el resultado en °C.
 * @return  DS18B20_OK o DS18B20_ERR_NO_DEVICE.
 */
unsigned char DS18B20_ReadTemperature(float *temperature) {
    unsigned char status;
    unsigned char lsb, msb;
    unsigned int raw;
    signed int raw_signed;

    status = DS18B20_Reset();
    if (status != DS18B20_OK) {
        return DS18B20_ERR_NO_DEVICE;
    }

    OW_WriteByte(DS18B20_CMD_SKIP_ROM);       // 0xCC
    OW_WriteByte(DS18B20_CMD_READ_SCRATCHPAD);// 0xBE

    lsb = OW_ReadByte();   // Byte 0: temperatura LSB
    msb = OW_ReadByte();   // Byte 1: temperatura MSB (con signo)

    /* Combina los dos bytes en un valor de 16 bits */
    raw = ((unsigned int)msb << 8) | lsb;

    /* Convierte a entero con signo (para manejar temperaturas negativas) */
    raw_signed = (signed int)raw;

    /* Multiplica por la resolución de 12 bits: 1 LSB = 0.0625 °C */
    *temperature = (float)raw_signed * 0.0625f;

    return DS18B20_OK;
}
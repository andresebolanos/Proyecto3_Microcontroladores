#include "MAX30102_Libreria.h"

/* -------------------------------------------------------------------------
 * MAX30102_WriteReg
 * Envia al sensor: [START] [ADDR_W] [REG] [VAL] [STOP]
 * Con esto le decimos al sensor "en el registro REG guarda el valor VAL".
 * ------------------------------------------------------------------------- */
void MAX30102_WriteReg(unsigned char reg, unsigned char val) {
    I2C_Start(MAX30102_ADDR_W);
    I2C_Write(reg);
    I2C_Write(val);
    I2C_Stop();
}

/* -------------------------------------------------------------------------
 * MAX30102_ReadReg
 * Para leer un registro hay que hacer dos transferencias I2C:
 *   1. Escritura: le decimos al sensor que registro queremos leer
 *   2. Restart + lectura: cambiamos a modo lectura sin soltar el bus
 *      y el sensor nos envia el byte de ese registro
 * El NACK al final le indica al sensor que no mande mas bytes.
 * ------------------------------------------------------------------------- */
unsigned char MAX30102_ReadReg(unsigned char reg) {
    unsigned char valor;
    I2C_Start(MAX30102_ADDR_W);
    I2C_Write(reg);          /* Apunta al registro que queremos leer */
    I2C_Restart();           /* Cambia direccion sin soltar el bus   */
    I2C_Write(MAX30102_ADDR_R);
    valor = I2C_Read(0);     /* 0 = NACK, le dice al sensor "ya es suficiente" */
    I2C_Stop();
    return valor;
}

/* -------------------------------------------------------------------------
 * MAX30102_Reset
 * Escribe un 1 en el bit RESET del registro MODE_CONFIG.
 * El sensor devuelve todos sus registros a los valores de fabrica.
 * El bit se limpia solo cuando el reset termina, por eso esperamos 10 ms.
 * ------------------------------------------------------------------------- */
void MAX30102_Reset(void) {
    MAX30102_WriteReg(MAX_REG_MODE_CONFIG, 0x40);
    __delay_ms(10);
}

/* -------------------------------------------------------------------------
 * MAX30102_Shutdown
 * Pone el bit SHDN del registro MODE_CONFIG en 1.
 * Los LEDs se apagan y el sensor deja de medir, pero la configuracion
 * se conserva en los registros internos.
 * Primero leemos el registro para no borrar los demas bits que ya tenia.
 * ------------------------------------------------------------------------- */
void MAX30102_Shutdown(void) {
    unsigned char reg = MAX30102_ReadReg(MAX_REG_MODE_CONFIG);
    MAX30102_WriteReg(MAX_REG_MODE_CONFIG, reg | 0x80);
}

/* -------------------------------------------------------------------------
 * MAX30102_Wakeup
 * Limpia el bit SHDN para reactivar el sensor.
 * Igual que Shutdown, leemos primero para conservar los demas bits.
 * ------------------------------------------------------------------------- */
void MAX30102_Wakeup(void) {
    unsigned char reg = MAX30102_ReadReg(MAX_REG_MODE_CONFIG);
    MAX30102_WriteReg(MAX_REG_MODE_CONFIG, reg & 0x7F);
}

/* -------------------------------------------------------------------------
 * MAX30102_Init
 * Configura el sensor paso a paso para trabajar en modo HR (solo LED Rojo).
 * En este modo la FIFO produce 3 bytes por muestra (un solo canal).
 * ------------------------------------------------------------------------- */
void MAX30102_Init(void) {
    /* Empezamos desde cero para evitar configuraciones basura en los registros */
    MAX30102_Reset();

    /* FIFO_CONFIG:
     * - SMP_AVE = 2: promediar 2 muestras consecutivas reduce el ruido
     *   sin bajar demasiado la frecuencia de actualizacion
     * - FIFO_ROLLOVER_EN = 1 (bit 4): si la FIFO se llena, el sensor
     *   sobreescribe las muestras mas viejas en lugar de detenerse.
     *   Asi el PIC siempre lee datos recientes aunque tarde en leer.
     * - FIFO_A_FULL = 15 (bits [3:0]): genera interrupcion cuando quedan
     *   17 espacios libres (no la usamos aqui, pero es buena practica) */
    MAX30102_WriteReg(MAX_REG_FIFO_CONFIG, MAX_SMP_AVE_2 | 0x10 | 0x0F);

    /* MODE_CONFIG: modo HR -> solo el LED Rojo queda activo.
     * La FIFO recibe 3 bytes por muestra (un canal de 18 bits). */
    MAX30102_WriteReg(MAX_REG_MODE_CONFIG, MAX_MODE_HR);

    /* SPO2_CONFIG (también aplica en modo HR):
     * - ADC_RGE = 4096 nA: rango medio, buena relacion senal/ruido
     * - SR = 100 SPS: 100 muestras por segundo. Es suficiente para
     *   capturar el pulso humano (maximo ~200 LPM = ~3.3 Hz)
     * - LED_PW = 411 us: el pulso mas largo da la mayor resolucion
     *   del ADC (18 bits) y la mayor cantidad de luz emitida */
    MAX30102_WriteReg(MAX_REG_SPO2_CONFIG, MAX_ADC_RGE_4096 | MAX_SR_100 | MAX_PW_411);

    /* --- MODIFICA ESTA LÍNEA --- */
    // Cambiamos MAX_LED_HIGH (0x7F) por un valor manual bajo como 0x14 (~4.0 mA)
    // Si notas que el valor RAW sigue en 262143, puedes bajarlo a 0x0B (~2.2 mA)
    MAX30102_WriteReg(MAX_REG_LED1_PA, 0x14); 
    
    MAX30102_WriteReg(MAX_REG_LED2_PA, 0x00); // El LED IR sigue apagado

    /* Limpiamos la FIFO... */
    MAX30102_ClearFIFO();
    __delay_ms(50);
}

/* -------------------------------------------------------------------------
 * MAX30102_ClearFIFO
 * La FIFO tiene tres punteros: escritura (WR), lectura (RD) y overflow (OVF).
 * Escribir 0 en los tres los sincroniza, lo que equivale a vaciar la FIFO.
 * Usar esto antes de empezar a medir o despues de un overflow.
 * ------------------------------------------------------------------------- */
void MAX30102_ClearFIFO(void) {
    MAX30102_WriteReg(MAX_REG_FIFO_WR_PTR, 0x00);
    MAX30102_WriteReg(MAX_REG_OVF_COUNTER, 0x00);
    MAX30102_WriteReg(MAX_REG_FIFO_RD_PTR, 0x00);
}

/* -------------------------------------------------------------------------
 * MAX30102_SamplesAvailable
 * La FIFO es un buffer circular de 32 posiciones.
 * El sensor mueve WR_PTR cada vez que graba una muestra nueva.
 * El PIC mueve RD_PTR cada vez que lee una muestra.
 * La diferencia entre ambos (modulo 32) es la cantidad de muestras
 * que el sensor ya guardo pero el PIC todavia no leyo.
 * ------------------------------------------------------------------------- */
unsigned char MAX30102_SamplesAvailable(void) {
    unsigned char wr = MAX30102_ReadReg(MAX_REG_FIFO_WR_PTR);
    unsigned char rd = MAX30102_ReadReg(MAX_REG_FIFO_RD_PTR);
    return (wr - rd) & 0x1F;   /* & 0x1F es el modulo 32, funciona aunque WR haya dado la vuelta */
}

/* -------------------------------------------------------------------------
 * MAX30102_ReadSample
 * Lee una muestra de 18 bits del canal Rojo desde la FIFO.
 *
 * El sensor envia la muestra en 3 bytes (24 bits en total):
 *   Byte 0: bits [23:16] -> solo los 2 bits bajos son utiles ([17:16])
 *   Byte 1: bits [15:8]
 *   Byte 2: bits [7:0]
 * Los 6 bits superiores del Byte 0 siempre son 0 (datasheet pag. 22).
 *
 * El ACK/NACK en I2C_Read indica al sensor si debe seguir enviando:
 *   ACK  (1) = "entendido, manda el siguiente byte"
 *   NACK (0) = "ya termina, no mandes mas"
 * El NACK debe ir obligatoriamente en el ULTIMO byte leido.
 * Si se manda ACK en el ultimo byte, el sensor sigue esperando transmitir
 * y el bus queda bloqueado hasta el STOP.
 * ------------------------------------------------------------------------- */
unsigned char MAX30102_ReadSample(MAX30102_Sample *s) {
    unsigned char b0, b1, b2;

    if(MAX30102_SamplesAvailable() == 0) return 0;

    I2C_Start(MAX30102_ADDR_W);
    I2C_Write(MAX_REG_FIFO_DATA);
    I2C_Restart();
    I2C_Write(MAX30102_ADDR_R);

    b0 = I2C_Read(1);   /* Byte [23:16] -- ACK, vienen mas bytes  */
    b1 = I2C_Read(1);   /* Byte [15:8]  -- ACK, viene un byte mas */
    b2 = I2C_Read(0);   /* Byte [7:0]   -- NACK, fin de lectura   */

    I2C_Stop();

    /* Ensamblamos los 18 bits utiles:
     * (b0 & 0x03) descarta los 6 bits superiores que siempre son 0 */
    s->red = ((unsigned long)(b0 & 0x03) << 16) |
             ((unsigned long)b1          <<  8) |
             ((unsigned long)b2);

    s->ir = 0;   /* LED IR apagado en modo HR, este campo no se usa */

    return 1;
}

/* -------------------------------------------------------------------------
 * MAX30102_ReadTempRaw
 * Mide la temperatura interna del chip (util para compensacion o debug).
 * ADVERTENCIA: bloquea el CPU hasta 50 ms mientras espera la conversion.
 * No usar dentro del bucle principal de deteccion de latidos.
 *
 * La temperatura se entrega en dos registros:
 *   TEMP_INT:  parte entera con signo (complemento a 2, -128 a +127 C)
 *   TEMP_FRAC: parte fraccionaria en pasos de 0.0625 C (4 bits bajos)
 *
 * Para evitar flotantes, el resultado se devuelve escalado x10:
 *   ejemplo: 245 -> 24.5 C
 * ------------------------------------------------------------------------- */
signed int MAX30102_ReadTempRaw(void) {
    unsigned char espera;
    signed char   entero;
    unsigned char fraccion;

    /* Bit 0 de TEMP_CONFIG en 1 dispara la conversion.
     * El sensor lo limpia solo cuando termina (~30 ms tipico). */
    MAX30102_WriteReg(MAX_REG_TEMP_CONFIG, 0x01);

    espera = 50;
    while(espera--) {
        __delay_ms(1);
        if((MAX30102_ReadReg(MAX_REG_TEMP_CONFIG) & 0x01) == 0) break;
    }

    entero   = (signed char)MAX30102_ReadReg(MAX_REG_TEMP_INT);
    fraccion = MAX30102_ReadReg(MAX_REG_TEMP_FRAC) & 0x0F;

    /* fraccion * 0.0625 * 10 = fraccion * 0.625 = fraccion * 5 / 8
     * El +4 en el numerador redondea al entero mas cercano */
    return ((signed int)entero * 10) + ((signed int)((fraccion * 5 + 4) / 8));
}

/* -------------------------------------------------------------------------
 * MAX30102_GetPartID / MAX30102_IsConnected
 * El registro PART_ID (0xFF) siempre vale 0x15 en el MAX30102.
 * Leerlo es la forma mas sencilla de verificar que el sensor esta
 * conectado y respondiendo correctamente antes de empezar a medir.
 * ------------------------------------------------------------------------- */
unsigned char MAX30102_GetPartID(void) {
    return MAX30102_ReadReg(MAX_REG_PART_ID);
}

unsigned char MAX30102_IsConnected(void) {
    return (MAX30102_GetPartID() == 0x15) ? 1 : 0;
}

unsigned char MAX30102_ProcesarBPM(unsigned long raw_red, unsigned char *bpm_out) {
    static unsigned long dc_filter = 0;
    static long last_ac = 0;
    static unsigned int sample_counter = 0;
    static unsigned int last_beat_time = 0;
    
    static long threshold = 0;
    static long max_val = -999999;
    static long min_val = 999999;
    
    static unsigned char bpm_buffer[8] = {0,0,0,0,0,0,0,0}; 
    static unsigned char buffer_idx = 0;

    if (raw_red < 15000) { 
        dc_filter = 0; last_beat_time = 0; sample_counter = 0;
        max_val = -999999; min_val = 999999; threshold = 0;
        for(unsigned char i=0; i<8; i++) bpm_buffer[i] = 0;
        *bpm_out = 0; return 0;
    }

    if (dc_filter == 0) dc_filter = raw_red << 4;

    dc_filter = (dc_filter * 63 + (raw_red << 4)) / 64;
    long ac_signal = (raw_red << 4) - dc_filter;
    ac_signal = ac_signal >> 4; 

    sample_counter++;

    if (ac_signal > max_val) max_val = ac_signal;
    if (ac_signal < min_val) min_val = ac_signal;

    if (sample_counter % 50 == 0) {
        threshold = min_val + ((max_val - min_val) * 2 / 3); 
        
        if ((max_val - min_val) < 40) threshold = 0; 
        max_val = -999999; min_val = 999999;
    }

    unsigned char latido_detectado = 0;
    
    
    if (ac_signal > threshold && last_ac <= threshold && (sample_counter - last_beat_time) > 45) {
        
        if (last_beat_time > 0) {
            unsigned int intervalo_muestras = sample_counter - last_beat_time;

            
            if (intervalo_muestras >= 45 && intervalo_muestras <= 100) {
                
                
                unsigned char raw_bpm = (unsigned char)(6000 / intervalo_muestras);

                bpm_buffer[buffer_idx] = raw_bpm;
                buffer_idx = (buffer_idx + 1) % 8; // Ciclo sobre 8

                unsigned int suma = 0;
                unsigned char lecturas_validas = 0;
                for (unsigned char i = 0; i < 8; i++) {
                    if (bpm_buffer[i] > 0) {
                        suma += bpm_buffer[i];
                        lecturas_validas++;
                    }
                }

                if (lecturas_validas > 0) {
                    *bpm_out = suma / lecturas_validas;
                    latido_detectado = 1; 
                }
            }
        }
        last_beat_time = sample_counter;
    }

    last_ac = ac_signal;
    return latido_detectado;
}
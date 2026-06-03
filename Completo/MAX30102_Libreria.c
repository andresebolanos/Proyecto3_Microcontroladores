/**
 * @file MAX30102_Libreria.c
 * @brief Implementación del driver y algoritmo DSP para el oxímetro MAX30102.
 * @details Administra la comunicación de bajo nivel I2C con los registros del sensor, 
 * maneja la estructura de cola de la memoria FIFO interna, y aplica filtros digitales 
 * (Pasa-Altos, Pasa-Bajos) para el cálculo de latidos por minuto (BPM).
 * 
 * @author  Andres Bolaños
 * @date    2026
 * @version 1.0
 */

#include "MAX30102_Libreria.h"

/* =========================================
 * FUNCIONES DE BAJO NIVEL (REGISTROS)
 * ========================================= */

/**
 * @defgroup MAX_LowLevel Comunicación con Registros
 * @brief Funciones de lectura/escritura directa de registros I2C.
 * @{
 */

/**
 * @brief Escribe un valor en un registro interno del sensor.
 * @param reg Dirección del registro destino.
 * @param val Valor de configuración a escribir (8 bits).
 * @details Ejecuta la secuencia: [START] -> [ADDR_WRITE] -> [REG] -> [VAL] -> [STOP].
 */
void MAX30102_WriteReg(unsigned char reg, unsigned char val) {
    I2C_Start(MAX30102_ADDR_W);
    I2C_Write(reg);
    I2C_Write(val);
    I2C_Stop();
}

/**
 * @brief Lee el valor de un registro interno del sensor.
 * @param reg Dirección del registro a consultar.
 * @return El byte extraído del registro consultado.
 * @details Requiere una secuencia de lectura combinada usando I2C_Restart 
 *          para no liberar el bus entre la escritura de la dirección y la lectura.
 */
unsigned char MAX30102_ReadReg(unsigned char reg) {
    unsigned char valor;
    I2C_Start(MAX30102_ADDR_W);
    I2C_Write(reg);          
    I2C_Restart();           
    I2C_Write(MAX30102_ADDR_R);
    valor = I2C_Read(0);     /* 0 = NACK: indica al sensor que finalice el envío */
    I2C_Stop();
    return valor;
}

/** @} */ // Fin de MAX_LowLevel

/* =========================================
 * FUNCIONES DE CONTROL DEL SENSOR
 * ========================================= */

/**
 * @defgroup MAX_Control Control del Sensor
 * @brief Funciones de configuración y gestión del estado del MAX30102.
 * @{
 */

/**
 * @brief Realiza un reseteo por software de toda la máquina de estados del sensor.
 * @details Escribe el bit RESET en MODE_CONFIG. El sensor retorna todos sus registros
 * a valores de fábrica y limpia la FIFO. Incluye un retardo de estabilización de 10ms.
 */
void MAX30102_Reset(void) {
    MAX30102_WriteReg(MAX_REG_MODE_CONFIG, 0x40);
    __delay_ms(10);
}

/**
 * @brief Coloca el sensor en modo de ultrabajo consumo (Modo Sleep de hardware).
 * @details Pone el bit SHDN (Shutdown) en '1' preservando el resto de bits de configuración. 
 * Los LEDs se apagan y el ADC se detiene. El consumo se reduce a ~1µA.
 */
void MAX30102_Shutdown(void) {

    unsigned char reg = MAX30102_ReadReg(MAX_REG_MODE_CONFIG); /**< Registro MODE_CONFIG actual */

    MAX30102_WriteReg(MAX_REG_MODE_CONFIG, reg | 0x80);
}

/**
 * @brief Despierta al sensor del modo de bajo consumo.
 * @details Limpia el bit SHDN restaurando las mediciones normales con la configuración guardada.
 */
void MAX30102_Wakeup(void) {

    unsigned char reg = MAX30102_ReadReg(MAX_REG_MODE_CONFIG); /**< Registro MODE_CONFIG actual */

    MAX30102_WriteReg(MAX_REG_MODE_CONFIG, reg & 0x7F);
}

/**
 * @brief Inicializa el hardware biomédico para la captación de pulsaciones (Modo HR).
 * @details Ejecuta la configuración crítica:
 * - Modo Rollover de FIFO activo (previene bloqueos por buffer lleno).
 * - Promediado hardware de 2 muestras (SMP_AVE_2).
 * - ADC a 4096nA, Frecuencia a 100 Hz, Pulso a 411us (Resolución de 18 bits).
 * - Configura la corriente de excitación del LED Rojo (~4.0 mA, baja saturación).
 * @post El sensor comienza a tomar muestras inmediatamente después de esta función.
 */
void MAX30102_Init(void) {
    MAX30102_Reset();

    /* FIFO_CONFIG: SMP_AVE_2 | FIFO_ROLLOVER_EN | FIFO_EMPTY_OVF_CLR */
    MAX30102_WriteReg(MAX_REG_FIFO_CONFIG, MAX_SMP_AVE_2 | 0x10 | 0x0F);
    MAX30102_WriteReg(MAX_REG_MODE_CONFIG, MAX_MODE_HR);
    MAX30102_WriteReg(MAX_REG_SPO2_CONFIG, MAX_ADC_RGE_4096 | MAX_SR_100 | MAX_PW_411);

    /* Corriente del LED configurada manualmente baja (~4.0 mA) para evitar saturación del ADC a 262143 */
    /* 0x14 = 20 decimal -> 20 * 0.2mA = 4.0 mA */
    MAX30102_WriteReg(MAX_REG_LED1_PA, 0x14); 
    MAX30102_WriteReg(MAX_REG_LED2_PA, 0x00); /* LED IR apagado */

    MAX30102_ClearFIFO();
    __delay_ms(50);
}

/**
 * @brief Sincroniza los punteros internos de la memoria FIFO a cero.
 * @details Resetea RD_PTR, WR_PTR y OVF_COUNTER. Efectivamente vacía los datos remanentes.
 * @note Es recomendable llamar a esta función después de cada pausa prolongada en la lectura.
 */
void MAX30102_ClearFIFO(void) {
    MAX30102_WriteReg(MAX_REG_FIFO_WR_PTR, 0x00);
    MAX30102_WriteReg(MAX_REG_OVF_COUNTER, 0x00);
    MAX30102_WriteReg(MAX_REG_FIFO_RD_PTR, 0x00);
}

/**
 * @brief Calcula el número de datos crudos acumulados en el buffer del sensor.
 * @return Cantidad de muestras disponibles para lectura (De 0 a 31).
 * @note La FIFO del MAX30102 tiene capacidad para 32 muestras.
 */
unsigned char MAX30102_SamplesAvailable(void) {

    unsigned char wr = MAX30102_ReadReg(MAX_REG_FIFO_WR_PTR); /**< Puntero de escritura */
    unsigned char rd = MAX30102_ReadReg(MAX_REG_FIFO_RD_PTR); /**< Puntero de lectura */

    return (wr - rd) & 0x1F;
}

/**
 * @brief Extrae una muestra en crudo (18 bits) de la FIFO y la almacena en memoria RAM del PIC.
 * @param s Puntero a la estructura MAX30102_Sample donde se guardarán los resultados.
 * @return 1 si la lectura fue exitosa, 0 si la FIFO estaba vacía.
 * @details Realiza un modo "Burst Read" por I2C extrayendo los 3 bytes pertenecientes 
 * al canal Rojo. Los 6 bits superiores del primer byte son descartados mediante 
 * máscara (b0 & 0x03), ya que son bits de control no utilizados en modo HR.
 * 
 * Formato de los 3 bytes recibidos (Big Endian):
 * - Byte 0: D[17:10] (solo D[17:16] son válidos, D[15:10] son 0)
 * - Byte 1: D[9:2]
 * - Byte 2: D[1:0] (bits bajos)
 */
unsigned char MAX30102_ReadSample(MAX30102_Sample *s) {
    unsigned char b0; /**< Byte alto de la muestra */
    unsigned char b1; /**< Byte medio de la muestra */
    unsigned char b2; /**< Byte bajo de la muestra */

    if(MAX30102_SamplesAvailable() == 0) return 0;

    I2C_Start(MAX30102_ADDR_W);
    I2C_Write(MAX_REG_FIFO_DATA);
    I2C_Restart();
    I2C_Write(MAX30102_ADDR_R);

    /* Burst read: 3 bytes para el modo HR (solo LED rojo) */
    b0 = I2C_Read(1);   /* Byte Alto: se responde con ACK para continuar */
    b1 = I2C_Read(1);   /* Byte Medio: se responde con ACK para continuar */
    b2 = I2C_Read(0);   /* Byte Bajo: se responde con NACK para finalizar */

    I2C_Stop();

    /* Ensamblado de los 18 bits útiles. Máscara b0 & 0x03 ignora los 6 MSB */
    s->red = ((unsigned long)(b0 & 0x03) << 16) |
             ((unsigned long)b1          <<  8) |
             ((unsigned long)b2);
    s->ir = 0;   /* Modo HR: IR desactivado */

    return 1;
}

/**
 * @brief Dispara y retorna la medición de la temperatura interna del chip MAX30102.
 * @return Temperatura entera multiplicada por 10 (Ejemplo: 36.5 °C retorna 365).
 * @warning **Función bloqueante**: Retrasa el procesamiento hasta 50ms mientras 
 *          la conversión ADC finaliza. No llamar en bucles de tiempo crítico.
 * @note La resolución de temperatura es de 0.0625°C. La fracción de 4 bits se 
 *       multiplica por 0.0625. La fórmula de redondeo (frac*5+4)/8 aproxima *0.625.
 */
signed int MAX30102_ReadTempRaw(void) {

    unsigned char espera;    /**< Contador de espera */
    signed char entero;      /**< Parte entera de la temperatura */
    unsigned char fraccion;  /**< Parte fraccionaria */

    /* Iniciar conversión de temperatura */
    MAX30102_WriteReg(MAX_REG_TEMP_CONFIG, 0x01);

    /* Esperar hasta que el bit de habilitación se limpie (máx 50ms) */
    espera = 50;
    while(espera--) {
        __delay_ms(1);
        if((MAX30102_ReadReg(MAX_REG_TEMP_CONFIG) & 0x01) == 0) break;
    }

    entero   = (signed char)MAX30102_ReadReg(MAX_REG_TEMP_INT);
    fraccion = MAX30102_ReadReg(MAX_REG_TEMP_FRAC) & 0x0F;

    /* 
     * Conversión de fracción de 4 bits a décimas de grado:
     * - Cada bit fraccionario = 0.0625°C
     * - Valor real = entero + fraccion * 0.0625
     * - Para multiplicar por 10: (entero*10) + (fraccion * 0.625)
     * - 0.625 ≈ 5/8. Redondeo con +4 en numerador.
     */
    return ((signed int)entero * 10) + ((signed int)((fraccion * 5 + 4) / 8));
}

/**
 * @brief Obtiene el identificador de parte de fábrica.
 * @return El ID del chip (Siempre debe ser 0x15 para el MAX30102).
 */
unsigned char MAX30102_GetPartID(void) {
    return MAX30102_ReadReg(MAX_REG_PART_ID);
}

/**
 * @brief Diagnóstico de hardware en el bus I2C.
 * @return 1 si el sensor responde y es el modelo correcto, 0 en caso de fallo.
 */
unsigned char MAX30102_IsConnected(void) {
    return (MAX30102_GetPartID() == 0x15) ? 1 : 0;
}

/** @} */ // Fin de MAX_Control

/* =========================================
 * DSP ALGORITMO DE DETECCIÓN DE BPM
 * ========================================= */

/**
 * @defgroup MAX_DSP Procesamiento Digital de Señales
 * @brief Algoritmo en tiempo real para detección de latidos cardíacos.
 * @details Este módulo implementa un filtro de señal de pulso, detección de picos
 *          por umbral dinámico y cálculo de frecuencia cardíaca (BPM).
 * @{
 */

/**
 * @brief DSP Algoritmo matemático para estimar los Latidos por Minuto (BPM).
 * @param raw_red Dato óptico bruto extraído directamente del sensor en el bucle principal.
 * @param bpm_out Puntero donde se almacenará el valor final y estable de los BPM.
 * @return 1 si se detectó un pico sistólico válido en esta muestra, 0 en fase de búsqueda.
 * 
 * @details Este es el corazón matemático del firmware. Ejecuta las siguientes etapas:
 * 
 * **1. Detección de ausencia de dedo:**
 *    - Si raw_red < 15000, asume que no hay dedo y resetea todos los estados internos.
 * 
 * **2. Filtro Pasa-Altos recursivo (DC Removal):**
 *    - dc_filter = (dc_filter * 31 + (raw_red << 4)) / 32
 *    - Extrae la señal AC (pulsátil) eliminando el componente de luz continua.
 *    - El desplazamiento <<4 mejora la resolución del filtro.
 * 
 * **3. Filtro Pasa-Bajos (Suavizado):**
 *    - filtered_ac = (filtered_ac * 3 + ac_signal) / 4
 *    - Atenúa ruido de alta frecuencia y aísla la onda pleth fundamental.
 * 
 * **4. Seguidor de Envolvente Dinámico:**
 *    - max_val y min_val siguen la señal con constante de tiempo ajustable.
 *    - Umbral = min_val + (amplitud * 0.6) -> 60% del pico a pico.
 * 
 * **5. Detector de Cruce de Umbral:**
 *    - Detecta cuando filtered_ac cruza el umbral ascendente.
 *    - Implementa blanking temporal (20 muestras) para evitar dobles detecciones.
 * 
 * **6. Validación Fisiológica:**
 *    - Intervalo entre latidos debe estar entre 20 y 75 muestras (≈40-150 BPM reales).
 *    - Filtro de consistencia con buffer circular de 4 BPM previos.
 *    - Limita variaciones abruptas (>±20 BPM) para estabilidad.
 * 
 * **7. Salida Promediada:**
 *    - Retorna el promedio de los últimos 4 BPM válidos.
 * 
 * @note La frecuencia de muestreo configurada es de 100 Hz. Por lo tanto:
 *       - 20 muestras = 200ms (300 BPM máximo teórico)
 *       - 75 muestras = 750ms (80 BPM mínimo teórico)
 * 
 * @warning Los valores de umbrales (15000, 20, 75, 40, 6/10) fueron calibrados empíricamente
 *          con un LED Rojo a 4.0mA. Cambios en la corriente del LED pueden requerir reajustes.
 */
unsigned char MAX30102_ProcesarBPM(unsigned long raw_red, unsigned char *bpm_out) {

    static unsigned long dc_filter = 0;            /**< Filtro DC */
    static long filtered_ac = 0;                   /**< Señal suavizada */
    static long last_ac = 0;                       /**< Valor AC previo */
    static unsigned int sample_counter = 0;        /**< Contador de muestras */
    static unsigned int last_beat_time = 0;        /**< Último latido detectado */
    static long max_val = 0;                       /**< Envolvente superior */
    static long min_val = 0;                       /**< Envolvente inferior */
    static long threshold = 0;                     /**< Umbral dinámico */
    static unsigned char bpm_buffer[4] = {0,0,0,0}; /**< Historial BPM */
    static unsigned char buffer_idx = 0;           /**< Índice circular */
    static unsigned char primer_latido = 1;        /**< Bandera de primer latido */
    

    /* =========================================
     * LIMPIEZA DE ESTADOS (Dedo ausente)
     * ========================================= */
    if (raw_red < MAX_MIN_SIGNAL) { 
        dc_filter = 0; 
        filtered_ac = 0; 
        last_beat_time = 0; 
        sample_counter = 0;
        max_val = 0; 
        min_val = 0; 
        threshold = 0;
        for(unsigned char i=0; i<MAX_BPM_BUFFER_SIZE; i++) bpm_buffer[i] = 0;
        primer_latido = 1;
        *bpm_out = 0; 
        return 0;
    }

    /* =========================================
     * ADAPTACIÓN INICIAL
     * ========================================= */
    if (dc_filter == 0) {
        dc_filter = raw_red << 4;
        filtered_ac = 0;
        max_val = 50; 
        min_val = -50;
        sample_counter = 0;
        last_beat_time = 0;
    }

    /* =========================================
     * 1. FILTRO PASA-ALTOS (DC Removal)
     * ========================================= */
    dc_filter = (dc_filter * 31 + (raw_red << 4)) / 32;
    long ac_signal; /**< @brief Señal AC extraída tras filtro pasa-altos */
    ac_signal = (raw_red << 4) - dc_filter;    
    ac_signal = ac_signal >> 4;  /* Reajuste de escala */

    /* =========================================
     * 2. FILTRO PASA-BAJOS (Suavizado)
     * ========================================= */
    filtered_ac = (filtered_ac * 3 + ac_signal) / 4;

    sample_counter++;

    /* =========================================
     * 3. SEGUIDOR DE ENVOLVENTE DINÁMICO
     * ========================================= */
    if (filtered_ac > max_val) {
        max_val = filtered_ac;
    } else {
        max_val -= (max_val - filtered_ac) / 16;  /* Decaimiento lento */
    }

    if (filtered_ac < min_val) {
        min_val = filtered_ac;
    } else {
        min_val += (filtered_ac - min_val) / 16;  /* Recuperación lenta */
    }

    /* Umbral en el 60% de la amplitud pico-pico */
    long amplitud; /**< @brief Amplitud pico-pico de la señal */
    amplitud = max_val - min_val;
    if (amplitud > MAX_AMPLITUDE_MIN) {
        threshold = min_val + (amplitud * 6 / 10); 
    } else {
        threshold = 0;  /* Señal muy débil, deshabilitar detección */
    }

    unsigned char latido_detectado = 0; /**< @brief 1 si se detectó un latido en esta muestra */
    
    /* =========================================
     * 4. DETECCIÓN DE PICO CARDÍACO
     * ========================================= */
    if (filtered_ac > threshold && last_ac <= threshold && 
        (sample_counter - last_beat_time) > MAX_MIN_BEAT_INTERVAL) {
        
        if (last_beat_time > 0) {
            unsigned int intervalo_muestras; /**< @brief Intervalo entre latidos (en muestras) */
            intervalo_muestras = sample_counter - last_beat_time;

            /* Validación dentro de rango fisiológico */
            if (intervalo_muestras >= MAX_MIN_BEAT_INTERVAL && 
                intervalo_muestras <= MAX_MAX_BEAT_INTERVAL) {
                
                /* Fórmula: (60 Hz) / (intervalo muestras * 0.01 s) * 60? No. 
                 * A 100 Hz: BPM = 6000 / intervalo_muestras */
                unsigned char raw_bpm;  /**< @brief BPM calculado directamente del intervalo */
                raw_bpm = (unsigned char)(6000 / intervalo_muestras);

                /* =========================================
                 * 5. FILTRADO DE CONSISTENCIA
                 * ========================================= */
                if (!primer_latido) {
                    unsigned int suma_temporal; /**< @brief Suma temporal para promedio */
                    suma_temporal = 0;
                    for (unsigned char i = 0; i < MAX_BPM_BUFFER_SIZE; i++) 
                        suma_temporal += bpm_buffer[i];
                    unsigned char promedio_actual;  /**< @brief Promedio de los últimos 4 BPM */
                    promedio_actual = suma_temporal / MAX_BPM_BUFFER_SIZE;
                    
                    /* Limita variaciones abruptas (arritmias falsas por movimiento) */
                    if (raw_bpm > promedio_actual + MAX_BPM_DEVIATION) 
                        raw_bpm = promedio_actual + (MAX_BPM_DEVIATION / 2);
                    else if (raw_bpm < promedio_actual - MAX_BPM_DEVIATION) 
                        raw_bpm = promedio_actual - (MAX_BPM_DEVIATION / 2);
                }

                /* Actualización del buffer histórico circular */
                if (primer_latido) {
                    for(unsigned char i = 0; i < MAX_BPM_BUFFER_SIZE; i++) 
                        bpm_buffer[i] = raw_bpm;
                    primer_latido = 0;
                } else {
                    bpm_buffer[buffer_idx] = raw_bpm;
                    buffer_idx = (buffer_idx + 1) % MAX_BPM_BUFFER_SIZE; 
                }

                /* Promedio final para estabilidad */
                unsigned int suma;  /**< @brief Suma de BPM para promedio final */
                suma = 0;
                for (unsigned char i = 0; i < MAX_BPM_BUFFER_SIZE; i++) {
                    suma += bpm_buffer[i];
                }

                *bpm_out = suma / MAX_BPM_BUFFER_SIZE;
                latido_detectado = 1; 
            }
        }
        last_beat_time = sample_counter;
    }

    last_ac = filtered_ac; 
    return latido_detectado;
}

/** @} */ // Fin de MAX_DSP
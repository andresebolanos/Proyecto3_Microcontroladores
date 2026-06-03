/**
 * @file MAX30102_Libreria.h
 * @brief Cabecera de la librería para el sensor biomédico óptico MAX30102.
 * @details Contiene el mapa de registros internos, macros de configuración 
 * (corrientes de LED, anchos de pulso, tasas de muestreo) y los prototipos 
 * de funciones para la lectura del pulso cardíaco y la gestión del bus I2C.
 * 
 * @author  Andres Bolaños
 * @date    2026
 * @version 1.0
 */

#ifndef MAX30102_LIBRERIA_H
#define MAX30102_LIBRERIA_H

#include <xc.h>
#include "OLED_Libreria.h"

#ifndef _XTAL_FREQ
#define _XTAL_FREQ 8000000
#endif

/**
 * @defgroup MAX_I2C Direcciones I2C
 * @brief Configuración de direcciones para comunicación con el MAX30102.
 * @{
 */

/** 
 * @brief Dirección I2C para operación de escritura.
 * @note Calculada como (0x57 << 1) | 0 = 0xAE
 */
#define MAX30102_ADDR_W   0xAE

/** 
 * @brief Dirección I2C para operación de lectura.
 * @note Calculada como (0x57 << 1) | 1 = 0xAF
 */
#define MAX30102_ADDR_R   0xAF

/** @} */ // Fin de MAX_I2C

/**
 * @defgroup MAX_Registers Mapa de Registros Internos
 * @brief Direcciones de los registros de configuración y estado del MAX30102.
 * @{
 */

#define MAX_REG_INT_STATUS1   0x00  /**< @brief Interrupt Status 1 */
#define MAX_REG_INT_STATUS2   0x01  /**< @brief Interrupt Status 2 */
#define MAX_REG_INT_ENABLE1   0x02  /**< @brief Interrupt Enable 1 */
#define MAX_REG_INT_ENABLE2   0x03  /**< @brief Interrupt Enable 2 */
#define MAX_REG_FIFO_WR_PTR   0x04  /**< @brief Puntero de escritura interno del sensor en la FIFO */
#define MAX_REG_OVF_COUNTER   0x05  /**< @brief Contador de muestras sobreescritas por FIFO llena */
#define MAX_REG_FIFO_RD_PTR   0x06  /**< @brief Puntero de lectura para el microcontrolador */
#define MAX_REG_FIFO_DATA     0x07  /**< @brief Puerto de salida de datos (Burst read) */
#define MAX_REG_FIFO_CONFIG   0x08  /**< @brief Configuración de la FIFO */
#define MAX_REG_MODE_CONFIG   0x09  /**< @brief Modo de operación */
#define MAX_REG_SPO2_CONFIG   0x0A  /**< @brief Configuración de SpO2 (ADC, frecuencia, pulso) */
#define MAX_REG_LED1_PA       0x0C  /**< @brief Corriente del LED Rojo (0 = 0mA, 255 = 51mA) */
#define MAX_REG_LED2_PA       0x0D  /**< @brief Corriente del LED Infrarrojo */
#define MAX_REG_TEMP_INT      0x1F  /**< @brief Parte entera de la temperatura */
#define MAX_REG_TEMP_FRAC     0x20  /**< @brief Parte fraccionaria de la temperatura (4 bits) */
#define MAX_REG_TEMP_CONFIG   0x21  /**< @brief Habilitación de medición de temperatura */
#define MAX_REG_REV_ID        0xFE  /**< @brief Revisión del hardware */
#define MAX_REG_PART_ID       0xFF  /**< @brief ID de fábrica (siempre 0x15) */

/** @} */ // Fin de MAX_Registers

/**
 * @defgroup MAX_Sampling Configuración de Muestreo (SMP_AVE)
 * @brief Define cuántas muestras adyacentes se promedian internamente antes de pasarlas a la FIFO.
 * @note A mayor promedio, menor ruido pero menor tasa efectiva de muestreo.
 * @{
 */

#define MAX_SMP_AVE_1    0x00  /**< @brief Sin promediado */
#define MAX_SMP_AVE_2    0x20  /**< @brief Promedio de 2 muestras */
#define MAX_SMP_AVE_4    0x40  /**< @brief Promedio de 4 muestras */
#define MAX_SMP_AVE_8    0x60  /**< @brief Promedio de 8 muestras */
#define MAX_SMP_AVE_16   0x80  /**< @brief Promedio de 16 muestras */
#define MAX_SMP_AVE_32   0xA0  /**< @brief Promedio de 32 muestras */

/** @} */ // Fin de MAX_Sampling

/**
 * @defgroup MAX_Modes Modos de Operación
 * @brief Modos de funcionamiento del sensor según los LEDs activados.
 * @{
 */

#define MAX_MODE_HR      0x02  /**< @brief Heart Rate: Solo LED Rojo (3 bytes/muestra) */
#define MAX_MODE_SPO2    0x03  /**< @brief SpO2: LED Rojo + IR (6 bytes/muestra) */
#define MAX_MODE_MULTI   0x07  /**< @brief Multi-LED: Control por slots avanzado */

/** @} */ // Fin de MAX_Modes

/**
 * @defgroup MAX_ADCRange Rango del ADC (SPO2_ADC_RGE)
 * @brief Configura el rango de corriente medido por el ADC interno.
 * @note Un rango mayor permite señales más fuertes pero con menos resolución relativa.
 * @{
 */

#define MAX_ADC_RGE_2048  0x00  /**< @brief ±2048 nA */
#define MAX_ADC_RGE_4096  0x20  /**< @brief ±4096 nA (Recomendado estándar) */
#define MAX_ADC_RGE_8192  0x40  /**< @brief ±8192 nA */
#define MAX_ADC_RGE_16384 0x60  /**< @brief ±16384 nA */

/** @} */ // Fin de MAX_ADCRange

/**
 * @defgroup MAX_SampleRate Frecuencia de Muestreo (SPO2_SR)
 * @brief Velocidad a la que se toman las muestras ópticas.
 * @{
 */

#define MAX_SR_50    0x00  /**< @brief 50 muestras/segundo */
#define MAX_SR_100   0x04  /**< @brief 100 muestras/segundo (Ideal para pulso humano) */
#define MAX_SR_200   0x08  /**< @brief 200 muestras/segundo */
#define MAX_SR_400   0x0C  /**< @brief 400 muestras/segundo */
#define MAX_SR_800   0x10  /**< @brief 800 muestras/segundo */
#define MAX_SR_1000  0x14  /**< @brief 1000 muestras/segundo */
#define MAX_SR_1600  0x18  /**< @brief 1600 muestras/segundo */
#define MAX_SR_3200  0x1C  /**< @brief 3200 muestras/segundo */

/** @} */ // Fin de MAX_SampleRate

/**
 * @defgroup MAX_PulseWidth Ancho de Pulso del LED (LED_PW)
 * @brief Duración del pulso del LED. Afecta directamente la resolución del ADC.
 * @note Mayor ancho de pulso = mayor resolución (hasta 18 bits) pero menor tasa máxima.
 * @{
 */

#define MAX_PW_69    0x00  /**< @brief 69 us -> 15 bits de resolución */
#define MAX_PW_118   0x01  /**< @brief 118 us -> 16 bits de resolución */
#define MAX_PW_215   0x02  /**< @brief 215 us -> 17 bits de resolución */
#define MAX_PW_411   0x03  /**< @brief 411 us -> 18 bits de resolución máxima */

/** @} */ // Fin de MAX_PulseWidth

/**
 * @defgroup MAX_LEDCurrent Corrientes Predefinidas de LEDs
 * @brief Valores comunes para el registro de corriente de LED.
 * @note 1 LSB = 0.2 mA. A mayor corriente, mayor penetración pero mayor consumo.
 * @{
 */

#define MAX_LED_LOW    0x1F  /**< @brief ~6.4 mA (valor bajo, para piel clara) */
#define MAX_LED_MED    0x47  /**< @brief ~14.2 mA (valor medio, estándar) */
#define MAX_LED_HIGH   0x7F  /**< @brief ~25.4 mA (valor alto, piel oscura) */
#define MAX_LED_MAX    0xFF  /**< @brief ~51.0 mA (máximo, alto consumo) */

/** @} */ // Fin de MAX_LEDCurrent

/**
 * @defgroup MAX_DSP_Parameters Parámetros del Algoritmo de BPM
 * @brief Constantes utilizadas en el procesamiento digital de señales.
 * @{
 */

#define MAX_BPM_BUFFER_SIZE 4      /**< @brief Tamaño del buffer circular de BPM */
#define MAX_MIN_SIGNAL      15000  /**< @brief Umbral mínimo de señal para detectar dedo presente */
#define MAX_MIN_BEAT_INTERVAL 20    /**< @brief Intervalo mínimo entre latidos (muestras) */
#define MAX_MAX_BEAT_INTERVAL 75    /**< @brief Intervalo máximo entre latidos (muestras) */
#define MAX_BPM_DEVIATION   20     /**< @brief Desviación máxima permitida entre latidos (BPM) */
#define MAX_AMPLITUDE_MIN   40     /**< @brief Amplitud mínima para considerar un latido válido */

/** @} */ // Fin de MAX_DSP_Parameters

/**
 * @struct MAX30102_Sample
 * @brief Contenedor de datos ópticos en crudo extraídos de la FIFO.
 * @details En modo HR (Heart Rate), el campo `ir` permanece en 0 ya que 
 *          el diodo infrarrojo está apagado para ahorrar energía.
 */
typedef struct {
    unsigned long red; /**< @brief Lectura de luz reflejada del canal LED Rojo (18 bits útiles) */
    unsigned long ir;  /**< @brief Lectura del canal Infrarrojo (0 en modo HR, no utilizado) */
} MAX30102_Sample;

/* =========================================
 * PROTOTIPOS DE FUNCIONES DE CONTROL
 * ========================================= */

/**
 * @defgroup MAX_Functions Funciones del Driver
 * @brief API completa para controlar y leer datos del sensor MAX30102.
 * @{
 */

/** @brief Inicializa completamente el sensor para modo Heart Rate. */
void MAX30102_Init(void);

/** @brief Ejecuta un reset por software del sensor. */
void MAX30102_Reset(void);

/** @brief Coloca el sensor en modo de ultra bajo consumo (sleep). */
void MAX30102_Shutdown(void);

/** @brief Despierta al sensor del modo de bajo consumo. */
void MAX30102_Wakeup(void);

/** 
 * @brief Lee un registro interno del sensor.
 * @param reg Dirección del registro a leer.
 * @return Valor actual del registro.
 */
unsigned char MAX30102_ReadReg(unsigned char reg);

/** 
 * @brief Escribe un valor en un registro interno del sensor.
 * @param reg Dirección del registro destino.
 * @param val Valor a escribir.
 */
void MAX30102_WriteReg(unsigned char reg, unsigned char val);

/** @brief Limpia todos los punteros de la FIFO interna. */
void MAX30102_ClearFIFO(void);

/** 
 * @brief Obtiene la cantidad de muestras disponibles en la FIFO.
 * @return Número de muestras pendientes de lectura (0 a 31).
 */
unsigned char MAX30102_SamplesAvailable(void);

/** 
 * @brief Lee una muestra completa desde la FIFO.
 * @param s Puntero a estructura donde se almacenará el resultado.
 * @return 1 si se leyó una muestra, 0 si la FIFO estaba vacía.
 */
unsigned char MAX30102_ReadSample(MAX30102_Sample *s);

/** 
 * @brief Lee la temperatura interna del chip.
 * @return Temperatura en décimas de grado Celsius (ej: 365 = 36.5°C).
 * @warning Función bloqueante: puede tardar hasta 50ms.
 */
signed int MAX30102_ReadTempRaw(void);

/** 
 * @brief Obtiene el ID de parte del fabricante.
 * @return Debe ser 0x15 si el sensor está operativo.
 */
unsigned char MAX30102_GetPartID(void);

/** 
 * @brief Verifica si el sensor está presente y responde correctamente.
 * @return 1 si el sensor está conectado, 0 en caso contrario.
 */
unsigned char MAX30102_IsConnected(void);

/** 
 * @brief Procesa una muestra cruda para estimar los latidos por minuto (BPM).
 * @param raw_red Valor crudo del canal rojo (18 bits).
 * @param bpm_out Puntero donde se almacenará el BPM calculado.
 * @return 1 si se detectó un latido válido, 0 en caso contrario.
 * @note Esta función debe llamarse para cada muestra obtenida del sensor.
 * @see MAX30102_ReadSample
 */
unsigned char MAX30102_ProcesarBPM(unsigned long raw_red, unsigned char *bpm_out);

/** @} */ // Fin de MAX_Functions

#endif /* MAX30102_LIBRERIA_H */
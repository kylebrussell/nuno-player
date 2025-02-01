#ifndef STM32H7XX_HAL_H
#define STM32H7XX_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Basic HAL status */
typedef enum {
    HAL_OK      = 0x00,
    HAL_ERROR   = 0x01,
    HAL_BUSY    = 0x02,
    HAL_TIMEOUT = 0x03
} HAL_StatusTypeDef;

/* =========================
   GPIO Definitions & Stubs
   ========================= */
typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET
} GPIO_PinState;

typedef struct {
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
} GPIO_InitTypeDef;

typedef struct {
    /* Stub: no actual registers in simulation */
} GPIO_TypeDef;

#define GPIOA ((GPIO_TypeDef *)0x40020000)
#define GPIOB ((GPIO_TypeDef *)0x40020400)

// GPIO Pin definitions
#define GPIO_PIN_0  ((uint16_t)0x0001)
#define GPIO_PIN_1  ((uint16_t)0x0002)
#define GPIO_PIN_2  ((uint16_t)0x0004)
#define GPIO_PIN_3  ((uint16_t)0x0008)
#define GPIO_PIN_4  ((uint16_t)0x0010)
#define GPIO_PIN_5  ((uint16_t)0x0020)

// GPIO Mode definitions
#define GPIO_MODE_INPUT              0x00000000U
#define GPIO_MODE_OUTPUT_PP          0x00000001U
#define GPIO_MODE_IT_RISING         0x10110000U
#define GPIO_MODE_IT_FALLING        0x10210000U
#define GPIO_MODE_IT_RISING_FALLING 0x10310000U

// GPIO Pull definitions
#define GPIO_NOPULL   0x00000000U
#define GPIO_PULLUP   0x00000001U
#define GPIO_PULLDOWN 0x00000002U

// GPIO Speed definitions
#define GPIO_SPEED_FREQ_LOW      0x00000000U
#define GPIO_SPEED_FREQ_MEDIUM   0x00000001U
#define GPIO_SPEED_FREQ_HIGH     0x00000002U
#define GPIO_SPEED_FREQ_VERY_HIGH 0x00000003U

/* =========================
   I2C Definitions & Stubs
   ========================= */
typedef struct {
    uint32_t Timing;
    uint32_t OwnAddress1;
    uint32_t AddressingMode;
    uint32_t DualAddressMode;
    uint32_t OwnAddress2;
    uint32_t GeneralCallMode;
    uint32_t NoStretchMode;
} I2C_InitTypeDef;

typedef struct {
    void* Instance;
    I2C_InitTypeDef Init;
    uint8_t* pBuffPtr;
    uint16_t XferSize;
    uint16_t XferCount;
} I2C_HandleTypeDef;

#define I2C1 ((void*)0x40005400)

// I2C Configuration
#define I2C_ADDRESSINGMODE_7BIT      0x00000001U
#define I2C_DUALADDRESS_DISABLE      0x00000000U
#define I2C_GENERALCALL_DISABLE      0x00000000U
#define I2C_NOSTRETCH_DISABLE        0x00000000U

/* =========================
   DMA Definitions & Stubs
   ========================= */
typedef enum {
    DMA_MEMORY_TO_PERIPH = 0,
    DMA_PERIPH_TO_MEMORY = 1
} DMA_Direction_TypeDef;

typedef enum {
    DMA_PINC_DISABLE = 0,
    DMA_PINC_ENABLE = 1
} DMA_PeriphInc_TypeDef;

typedef enum {
    DMA_MINC_DISABLE = 0,
    DMA_MINC_ENABLE = 1
} DMA_MemInc_TypeDef;

typedef enum {
    DMA_PDATAALIGN_BYTE = 0,
    DMA_PDATAALIGN_HALFWORD = 1,
    DMA_PDATAALIGN_WORD = 2
} DMA_PeriphDataAlignment_TypeDef;

typedef enum {
    DMA_MDATAALIGN_BYTE = 0,
    DMA_MDATAALIGN_HALFWORD = 1,
    DMA_MDATAALIGN_WORD = 2
} DMA_MemDataAlignment_TypeDef;

typedef enum {
    DMA_NORMAL = 0,
    DMA_CIRCULAR = 1
} DMA_Mode_TypeDef;

typedef enum {
    DMA_PRIORITY_LOW = 0,
    DMA_PRIORITY_MEDIUM = 1,
    DMA_PRIORITY_HIGH = 2,
    DMA_PRIORITY_VERY_HIGH = 3
} DMA_Priority_TypeDef;

#define DMA_CHANNEL_0  0
#define DMA_CHANNEL_1  1
#define DMA_CHANNEL_2  2
#define DMA_CHANNEL_3  3

typedef struct {
    uint32_t Channel;
    uint32_t Direction;
    uint32_t PeriphInc;
    uint32_t MemInc;
    uint32_t PeriphDataAlignment;
    uint32_t MemDataAlignment;
    uint32_t Mode;
    uint32_t Priority;
    uint32_t FIFOMode;
} DMA_InitTypeDef;

typedef struct {
    void* Instance;
    DMA_InitTypeDef Init;
} DMA_HandleTypeDef;

#define DMA1_Stream0 ((void*)0x40026010)
#define DMA1_Stream1 ((void*)0x40026028)

#define __HAL_RCC_DMA1_CLK_ENABLE()  do { } while(0)
#define DMA_IT_TC                    ((uint32_t)0x00000004)
#define DMA_IT_HT                    ((uint32_t)0x00000002)

/* =========================
   NVIC Definitions & Stubs
   ========================= */
typedef enum {
    EXTI0_IRQn = 6,
    EXTI1_IRQn = 7,
    EXTI2_IRQn = 8,
    EXTI3_IRQn = 9,
    EXTI4_IRQn = 10,
    DMA1_Stream0_IRQn = 11,
    DMA1_Stream1_IRQn = 12,
    I2C1_EV_IRQn = 31,
    I2C1_ER_IRQn = 32
} IRQn_Type;

/* =========================
   HAL Function Prototypes
   ========================= */
void HAL_Init(void);
void SystemClock_Config(void);

// GPIO Functions
void HAL_GPIO_Init(GPIO_TypeDef* GPIOx, GPIO_InitTypeDef* GPIO_Init);
void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void HAL_GPIO_EXTI_IRQHandler(uint16_t GPIO_Pin);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

// I2C Functions
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef* hi2c);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, 
                                         uint8_t* pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef* hi2c, uint16_t DevAddress,
                                        uint8_t* pData, uint16_t Size, uint32_t Timeout);

// DMA Functions
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef* hdma);
HAL_StatusTypeDef HAL_DMA_Start(DMA_HandleTypeDef* hdma, uint32_t SrcAddress, 
                               uint32_t DstAddress, uint32_t DataLength);
HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef* hdma, uint32_t SrcAddress,
                                  uint32_t DstAddress, uint32_t DataLength);
void HAL_DMA_IRQHandler(DMA_HandleTypeDef* hdma);

// NVIC Functions
void HAL_NVIC_SetPriority(IRQn_Type IRQn, uint32_t PreemptPriority, uint32_t SubPriority);
void HAL_NVIC_EnableIRQ(IRQn_Type IRQn);
void HAL_NVIC_DisableIRQ(IRQn_Type IRQn);

// Delay Functions
void HAL_Delay(uint32_t Delay);

// Clock Enable Macros
#define __HAL_RCC_GPIOA_CLK_ENABLE()  do { } while(0)
#define __HAL_RCC_GPIOB_CLK_ENABLE()  do { } while(0)
#define __HAL_RCC_I2C1_CLK_ENABLE()   do { } while(0)

#ifdef __cplusplus
}
#endif

#endif // STM32H7XX_HAL_H
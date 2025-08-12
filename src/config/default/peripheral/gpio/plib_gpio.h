/*******************************************************************************
  GPIO PLIB

  Company:
    Microchip Technology Inc.

  File Name:
    plib_gpio.h

  Summary:
    GPIO PLIB Header File

  Description:
    This library provides an interface to control and interact with Parallel
    Input/Output controller (GPIO) module.

*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/

#ifndef PLIB_GPIO_H
#define PLIB_GPIO_H

#include <device.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility

    extern "C" {

#endif
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Data types and constants
// *****************************************************************************
// *****************************************************************************


/*** Macros for DRV1_LED pin ***/
#define DRV1_LED_Set()               (LATGSET = (1<<9))
#define DRV1_LED_Clear()             (LATGCLR = (1<<9))
#define DRV1_LED_Toggle()            (LATGINV= (1<<9))
#define DRV1_LED_OutputEnable()      (TRISGCLR = (1<<9))
#define DRV1_LED_InputEnable()       (TRISGSET = (1<<9))
#define DRV1_LED_Get()               ((PORTG >> 9) & 0x1)
#define DRV1_LED_GetLatch()          ((LATG >> 9) & 0x1)
#define DRV1_LED_PIN                  GPIO_PIN_RG9

/*** Macros for PGOOD pin ***/
#define PGOOD_Get()               ((PORTB >> 5) & 0x1)
#define PGOOD_GetLatch()          ((LATB >> 5) & 0x1)
#define PGOOD_PIN                  GPIO_PIN_RB5

/*** Macros for FB_VLED2 pin ***/
#define FB_VLED2_Get()               ((PORTB >> 4) & 0x1)
#define FB_VLED2_GetLatch()          ((LATB >> 4) & 0x1)
#define FB_VLED2_PIN                  GPIO_PIN_RB4

/*** Macros for FB_VLED1 pin ***/
#define FB_VLED1_Get()               ((PORTB >> 3) & 0x1)
#define FB_VLED1_GetLatch()          ((LATB >> 3) & 0x1)
#define FB_VLED1_PIN                  GPIO_PIN_RB3

/*** Macros for V48V_EXT pin ***/
#define V48V_EXT_Get()               ((PORTB >> 2) & 0x1)
#define V48V_EXT_GetLatch()          ((LATB >> 2) & 0x1)
#define V48V_EXT_PIN                  GPIO_PIN_RB2

/*** Macros for ICSP_CLK pin ***/
#define ICSP_CLK_Set()               (LATBSET = (1<<1))
#define ICSP_CLK_Clear()             (LATBCLR = (1<<1))
#define ICSP_CLK_Toggle()            (LATBINV= (1<<1))
#define ICSP_CLK_OutputEnable()      (TRISBCLR = (1<<1))
#define ICSP_CLK_InputEnable()       (TRISBSET = (1<<1))
#define ICSP_CLK_Get()               ((PORTB >> 1) & 0x1)
#define ICSP_CLK_GetLatch()          ((LATB >> 1) & 0x1)
#define ICSP_CLK_PIN                  GPIO_PIN_RB1

/*** Macros for ICSP_DATA pin ***/
#define ICSP_DATA_Set()               (LATBSET = (1<<0))
#define ICSP_DATA_Clear()             (LATBCLR = (1<<0))
#define ICSP_DATA_Toggle()            (LATBINV= (1<<0))
#define ICSP_DATA_OutputEnable()      (TRISBCLR = (1<<0))
#define ICSP_DATA_InputEnable()       (TRISBSET = (1<<0))
#define ICSP_DATA_Get()               ((PORTB >> 0) & 0x1)
#define ICSP_DATA_GetLatch()          ((LATB >> 0) & 0x1)
#define ICSP_DATA_PIN                  GPIO_PIN_RB0

/*** Macros for DE_TERM pin ***/
#define DE_TERM_Set()               (LATBSET = (1<<6))
#define DE_TERM_Clear()             (LATBCLR = (1<<6))
#define DE_TERM_Toggle()            (LATBINV= (1<<6))
#define DE_TERM_OutputEnable()      (TRISBCLR = (1<<6))
#define DE_TERM_InputEnable()       (TRISBSET = (1<<6))
#define DE_TERM_Get()               ((PORTB >> 6) & 0x1)
#define DE_TERM_GetLatch()          ((LATB >> 6) & 0x1)
#define DE_TERM_PIN                  GPIO_PIN_RB6

/*** Macros for AUX_AN1 pin ***/
#define AUX_AN1_Get()               ((PORTB >> 7) & 0x1)
#define AUX_AN1_GetLatch()          ((LATB >> 7) & 0x1)
#define AUX_AN1_PIN                  GPIO_PIN_RB7

/*** Macros for PD_CTRL pin ***/
#define PD_CTRL_Set()               (LATBSET = (1<<8))
#define PD_CTRL_Clear()             (LATBCLR = (1<<8))
#define PD_CTRL_Toggle()            (LATBINV= (1<<8))
#define PD_CTRL_OutputEnable()      (TRISBCLR = (1<<8))
#define PD_CTRL_InputEnable()       (TRISBSET = (1<<8))
#define PD_CTRL_Get()               ((PORTB >> 8) & 0x1)
#define PD_CTRL_GetLatch()          ((LATB >> 8) & 0x1)
#define PD_CTRL_PIN                  GPIO_PIN_RB8

/*** Macros for TX_DATA pin ***/
#define TX_DATA_Set()               (LATBSET = (1<<9))
#define TX_DATA_Clear()             (LATBCLR = (1<<9))
#define TX_DATA_Toggle()            (LATBINV= (1<<9))
#define TX_DATA_OutputEnable()      (TRISBCLR = (1<<9))
#define TX_DATA_InputEnable()       (TRISBSET = (1<<9))
#define TX_DATA_Get()               ((PORTB >> 9) & 0x1)
#define TX_DATA_GetLatch()          ((LATB >> 9) & 0x1)
#define TX_DATA_PIN                  GPIO_PIN_RB9

/*** Macros for FB_DRV2 pin ***/
#define FB_DRV2_Get()               ((PORTB >> 10) & 0x1)
#define FB_DRV2_GetLatch()          ((LATB >> 10) & 0x1)
#define FB_DRV2_PIN                  GPIO_PIN_RB10

/*** Macros for FB_DRV1 pin ***/
#define FB_DRV1_Get()               ((PORTB >> 11) & 0x1)
#define FB_DRV1_GetLatch()          ((LATB >> 11) & 0x1)
#define FB_DRV1_PIN                  GPIO_PIN_RB11

/*** Macros for V48V pin ***/
#define V48V_Get()               ((PORTB >> 12) & 0x1)
#define V48V_GetLatch()          ((LATB >> 12) & 0x1)
#define V48V_PIN                  GPIO_PIN_RB12

/*** Macros for FB_010V pin ***/
#define FB_010V_Get()               ((PORTB >> 13) & 0x1)
#define FB_010V_GetLatch()          ((LATB >> 13) & 0x1)
#define FB_010V_PIN                  GPIO_PIN_RB13

/*** Macros for NTC pin ***/
#define NTC_Get()               ((PORTB >> 14) & 0x1)
#define NTC_GetLatch()          ((LATB >> 14) & 0x1)
#define NTC_PIN                  GPIO_PIN_RB14

/*** Macros for DE_COM pin ***/
#define DE_COM_Set()               (LATFSET = (1<<3))
#define DE_COM_Clear()             (LATFCLR = (1<<3))
#define DE_COM_Toggle()            (LATFINV= (1<<3))
#define DE_COM_OutputEnable()      (TRISFCLR = (1<<3))
#define DE_COM_InputEnable()       (TRISFSET = (1<<3))
#define DE_COM_Get()               ((PORTF >> 3) & 0x1)
#define DE_COM_GetLatch()          ((LATF >> 3) & 0x1)
#define DE_COM_PIN                  GPIO_PIN_RF3

/*** Macros for TEST_SW pin ***/
#define TEST_SW_Set()               (LATGSET = (1<<3))
#define TEST_SW_Clear()             (LATGCLR = (1<<3))
#define TEST_SW_Toggle()            (LATGINV= (1<<3))
#define TEST_SW_OutputEnable()      (TRISGCLR = (1<<3))
#define TEST_SW_InputEnable()       (TRISGSET = (1<<3))
#define TEST_SW_Get()               ((PORTG >> 3) & 0x1)
#define TEST_SW_GetLatch()          ((LATG >> 3) & 0x1)
#define TEST_SW_PIN                  GPIO_PIN_RG3

/*** Macros for RX_DATA pin ***/
#define RX_DATA_Set()               (LATGSET = (1<<2))
#define RX_DATA_Clear()             (LATGCLR = (1<<2))
#define RX_DATA_Toggle()            (LATGINV= (1<<2))
#define RX_DATA_OutputEnable()      (TRISGCLR = (1<<2))
#define RX_DATA_InputEnable()       (TRISGSET = (1<<2))
#define RX_DATA_Get()               ((PORTG >> 2) & 0x1)
#define RX_DATA_GetLatch()          ((LATG >> 2) & 0x1)
#define RX_DATA_PIN                  GPIO_PIN_RG2

/*** Macros for EN_48V_EXT pin ***/
#define EN_48V_EXT_Set()               (LATDSET = (1<<8))
#define EN_48V_EXT_Clear()             (LATDCLR = (1<<8))
#define EN_48V_EXT_Toggle()            (LATDINV= (1<<8))
#define EN_48V_EXT_OutputEnable()      (TRISDCLR = (1<<8))
#define EN_48V_EXT_InputEnable()       (TRISDSET = (1<<8))
#define EN_48V_EXT_Get()               ((PORTD >> 8) & 0x1)
#define EN_48V_EXT_GetLatch()          ((LATD >> 8) & 0x1)
#define EN_48V_EXT_PIN                  GPIO_PIN_RD8

/*** Macros for PHY_RST pin ***/
#define PHY_RST_Set()               (LATDSET = (1<<11))
#define PHY_RST_Clear()             (LATDCLR = (1<<11))
#define PHY_RST_Toggle()            (LATDINV= (1<<11))
#define PHY_RST_OutputEnable()      (TRISDCLR = (1<<11))
#define PHY_RST_InputEnable()       (TRISDSET = (1<<11))
#define PHY_RST_Get()               ((PORTD >> 11) & 0x1)
#define PHY_RST_GetLatch()          ((LATD >> 11) & 0x1)
#define PHY_RST_PIN                  GPIO_PIN_RD11

/*** Macros for PWM1 pin ***/
#define PWM1_Get()               ((PORTD >> 0) & 0x1)
#define PWM1_GetLatch()          ((LATD >> 0) & 0x1)
#define PWM1_PIN                  GPIO_PIN_RD0

/*** Macros for OC_OV pin ***/
#define OC_OV_Set()               (LATCSET = (1<<13))
#define OC_OV_Clear()             (LATCCLR = (1<<13))
#define OC_OV_Toggle()            (LATCINV= (1<<13))
#define OC_OV_OutputEnable()      (TRISCCLR = (1<<13))
#define OC_OV_InputEnable()       (TRISCSET = (1<<13))
#define OC_OV_Get()               ((PORTC >> 13) & 0x1)
#define OC_OV_GetLatch()          ((LATC >> 13) & 0x1)
#define OC_OV_PIN                  GPIO_PIN_RC13

/*** Macros for EN_48V pin ***/
#define EN_48V_Set()               (LATCSET = (1<<14))
#define EN_48V_Clear()             (LATCCLR = (1<<14))
#define EN_48V_Toggle()            (LATCINV= (1<<14))
#define EN_48V_OutputEnable()      (TRISCCLR = (1<<14))
#define EN_48V_InputEnable()       (TRISCSET = (1<<14))
#define EN_48V_Get()               ((PORTC >> 14) & 0x1)
#define EN_48V_GetLatch()          ((LATC >> 14) & 0x1)
#define EN_48V_PIN                  GPIO_PIN_RC14

/*** Macros for PWM2 pin ***/
#define PWM2_Get()               ((PORTD >> 4) & 0x1)
#define PWM2_GetLatch()          ((LATD >> 4) & 0x1)
#define PWM2_PIN                  GPIO_PIN_RD4

/*** Macros for CS_FLASH pin ***/
#define CS_FLASH_Set()               (LATDSET = (1<<5))
#define CS_FLASH_Clear()             (LATDCLR = (1<<5))
#define CS_FLASH_Toggle()            (LATDINV= (1<<5))
#define CS_FLASH_OutputEnable()      (TRISDCLR = (1<<5))
#define CS_FLASH_InputEnable()       (TRISDSET = (1<<5))
#define CS_FLASH_Get()               ((PORTD >> 5) & 0x1)
#define CS_FLASH_GetLatch()          ((LATD >> 5) & 0x1)
#define CS_FLASH_PIN                  GPIO_PIN_RD5

/*** Macros for CS_EEPROM pin ***/
#define CS_EEPROM_Set()               (LATDSET = (1<<7))
#define CS_EEPROM_Clear()             (LATDCLR = (1<<7))
#define CS_EEPROM_Toggle()            (LATDINV= (1<<7))
#define CS_EEPROM_OutputEnable()      (TRISDCLR = (1<<7))
#define CS_EEPROM_InputEnable()       (TRISDSET = (1<<7))
#define CS_EEPROM_Get()               ((PORTD >> 7) & 0x1)
#define CS_EEPROM_GetLatch()          ((LATD >> 7) & 0x1)
#define CS_EEPROM_PIN                  GPIO_PIN_RD7

/*** Macros for DRV2_LED pin ***/
#define DRV2_LED_Set()               (LATFSET = (1<<0))
#define DRV2_LED_Clear()             (LATFCLR = (1<<0))
#define DRV2_LED_Toggle()            (LATFINV= (1<<0))
#define DRV2_LED_OutputEnable()      (TRISFCLR = (1<<0))
#define DRV2_LED_InputEnable()       (TRISFSET = (1<<0))
#define DRV2_LED_Get()               ((PORTF >> 0) & 0x1)
#define DRV2_LED_GetLatch()          ((LATF >> 0) & 0x1)
#define DRV2_LED_PIN                  GPIO_PIN_RF0

/*** Macros for LED_STAT pin ***/
#define LED_STAT_Set()               (LATFSET = (1<<1))
#define LED_STAT_Clear()             (LATFCLR = (1<<1))
#define LED_STAT_Toggle()            (LATFINV= (1<<1))
#define LED_STAT_OutputEnable()      (TRISFCLR = (1<<1))
#define LED_STAT_InputEnable()       (TRISFSET = (1<<1))
#define LED_STAT_Get()               ((PORTF >> 1) & 0x1)
#define LED_STAT_GetLatch()          ((LATF >> 1) & 0x1)
#define LED_STAT_PIN                  GPIO_PIN_RF1


// *****************************************************************************
/* GPIO Port

  Summary:
    Identifies the available GPIO Ports.

  Description:
    This enumeration identifies the available GPIO Ports.

  Remarks:
    The caller should not rely on the specific numbers assigned to any of
    these values as they may change from one processor to the next.

    Not all ports are available on all devices.  Refer to the specific
    device data sheet to determine which ports are supported.
*/


#define    GPIO_PORT_B   (0)
#define    GPIO_PORT_C   (1)
#define    GPIO_PORT_D   (2)
#define    GPIO_PORT_E   (3)
#define    GPIO_PORT_F   (4)
#define    GPIO_PORT_G   (5)
typedef uint32_t GPIO_PORT;

// *****************************************************************************
/* GPIO Port Pins

  Summary:
    Identifies the available GPIO port pins.

  Description:
    This enumeration identifies the available GPIO port pins.

  Remarks:
    The caller should not rely on the specific numbers assigned to any of
    these values as they may change from one processor to the next.

    Not all pins are available on all devices.  Refer to the specific
    device data sheet to determine which pins are supported.
*/
#define    GPIO_PIN_RB0   (0)
#define    GPIO_PIN_RB1   (1)
#define    GPIO_PIN_RB2   (2)
#define    GPIO_PIN_RB3   (3)
#define    GPIO_PIN_RB4   (4)
#define    GPIO_PIN_RB5   (5)
#define    GPIO_PIN_RB6   (6)
#define    GPIO_PIN_RB7   (7)
#define    GPIO_PIN_RB8   (8)
#define    GPIO_PIN_RB9   (9)
#define    GPIO_PIN_RB10   (10)
#define    GPIO_PIN_RB11   (11)
#define    GPIO_PIN_RB12   (12)
#define    GPIO_PIN_RB13   (13)
#define    GPIO_PIN_RB14   (14)
#define    GPIO_PIN_RB15   (15)
#define    GPIO_PIN_RC12   (28)
#define    GPIO_PIN_RC13   (29)
#define    GPIO_PIN_RC14   (30)
#define    GPIO_PIN_RC15   (31)
#define    GPIO_PIN_RD0   (32)
#define    GPIO_PIN_RD1   (33)
#define    GPIO_PIN_RD2   (34)
#define    GPIO_PIN_RD3   (35)
#define    GPIO_PIN_RD4   (36)
#define    GPIO_PIN_RD5   (37)
#define    GPIO_PIN_RD6   (38)
#define    GPIO_PIN_RD7   (39)
#define    GPIO_PIN_RD8   (40)
#define    GPIO_PIN_RD9   (41)
#define    GPIO_PIN_RD10   (42)
#define    GPIO_PIN_RD11   (43)
#define    GPIO_PIN_RE0   (48)
#define    GPIO_PIN_RE1   (49)
#define    GPIO_PIN_RE2   (50)
#define    GPIO_PIN_RE3   (51)
#define    GPIO_PIN_RE4   (52)
#define    GPIO_PIN_RE5   (53)
#define    GPIO_PIN_RE6   (54)
#define    GPIO_PIN_RE7   (55)
#define    GPIO_PIN_RF0   (64)
#define    GPIO_PIN_RF1   (65)
#define    GPIO_PIN_RF3   (67)
#define    GPIO_PIN_RF4   (68)
#define    GPIO_PIN_RF5   (69)
#define    GPIO_PIN_RG2   (82)
#define    GPIO_PIN_RG3   (83)
#define    GPIO_PIN_RG6   (86)
#define    GPIO_PIN_RG7   (87)
#define    GPIO_PIN_RG8   (88)
#define    GPIO_PIN_RG9   (89)

    /* This element should not be used in any of the GPIO APIs.
       It will be used by other modules or application to denote that none of the GPIO Pin is used */
#define    GPIO_PIN_NONE     (-1)

typedef uint32_t GPIO_PIN;

typedef enum
{
  CN0_PIN = 1 << 0,
  CN1_PIN = 1 << 1,
  CN2_PIN = 1 << 2,
  CN3_PIN = 1 << 3,
  CN4_PIN = 1 << 4,
  CN5_PIN = 1 << 5,
  CN6_PIN = 1 << 6,
  CN7_PIN = 1 << 7,
  CN8_PIN = 1 << 8,
  CN9_PIN = 1 << 9,
  CN10_PIN = 1 << 10,
  CN11_PIN = 1 << 11,
  CN12_PIN = 1 << 12,
  CN13_PIN = 1 << 13,
  CN14_PIN = 1 << 14,
  CN15_PIN = 1 << 15,
  CN16_PIN = 1 << 16,
  CN17_PIN = 1 << 17,
  CN18_PIN = 1 << 18,
}CN_PIN;


void GPIO_Initialize(void);

// *****************************************************************************
// *****************************************************************************
// Section: GPIO Functions which operates on multiple pins of a port
// *****************************************************************************
// *****************************************************************************

uint32_t GPIO_PortRead(GPIO_PORT port);

void GPIO_PortWrite(GPIO_PORT port, uint32_t mask, uint32_t value);

uint32_t GPIO_PortLatchRead ( GPIO_PORT port );

void GPIO_PortSet(GPIO_PORT port, uint32_t mask);

void GPIO_PortClear(GPIO_PORT port, uint32_t mask);

void GPIO_PortToggle(GPIO_PORT port, uint32_t mask);

void GPIO_PortInputEnable(GPIO_PORT port, uint32_t mask);

void GPIO_PortOutputEnable(GPIO_PORT port, uint32_t mask);

// *****************************************************************************
// *****************************************************************************
// Section: GPIO Functions which operates on one pin at a time
// *****************************************************************************
// *****************************************************************************

static inline void GPIO_PinWrite(GPIO_PIN pin, bool value)
{
    GPIO_PortWrite((GPIO_PORT)(pin>>4), (uint32_t)(0x1UL) << (pin & 0xFU), (uint32_t)(value) << (pin & 0xFU));
}

static inline bool GPIO_PinRead(GPIO_PIN pin)
{
    return (bool)(((GPIO_PortRead((GPIO_PORT)(pin>>4U))) >> (pin & 0xFU)) & 0x1U);
}

static inline bool GPIO_PinLatchRead(GPIO_PIN pin)
{
    return (bool)((GPIO_PortLatchRead((GPIO_PORT)(pin>>4U)) >> (pin & 0xFU)) & 0x1U);
}

static inline void GPIO_PinToggle(GPIO_PIN pin)
{
    GPIO_PortToggle((GPIO_PORT)(pin>>4U), 0x1UL << (pin & 0xFU));
}

static inline void GPIO_PinSet(GPIO_PIN pin)
{
    GPIO_PortSet((GPIO_PORT)(pin>>4U), 0x1UL << (pin & 0xFU));
}

static inline void GPIO_PinClear(GPIO_PIN pin)
{
    GPIO_PortClear((GPIO_PORT)(pin>>4U), 0x1UL << (pin & 0xFU));
}

static inline void GPIO_PinInputEnable(GPIO_PIN pin)
{
    GPIO_PortInputEnable((GPIO_PORT)(pin>>4U), 0x1UL << (pin & 0xFU));
}

static inline void GPIO_PinOutputEnable(GPIO_PIN pin)
{
    GPIO_PortOutputEnable((GPIO_PORT)(pin>>4U), 0x1UL << (pin & 0xFU));
}


// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility

    }

#endif
// DOM-IGNORE-END
#endif // PLIB_GPIO_H

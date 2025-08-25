
#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "user.h"
#include "device.h"

// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility

extern "C" {

#endif
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Driver Configuration
// *****************************************************************************
/* SST26 Driver Instance Configuration */
#define DRV_SST26_INDEX                 (0U)
#define DRV_SST26_CLIENTS_NUMBER        (1U)
#define DRV_SST26_START_ADDRESS         (0x0U)
#define DRV_SST26_PAGE_SIZE             (256U)
#define DRV_SST26_ERASE_BUFFER_SIZE     (4096U)
#define DRV_SST26_CHIP_SELECT_PIN       SYS_PORT_PIN_RD5

//DOM-IGNORE-BEGIN
#ifdef __cplusplus
}
#endif
//DOM-IGNORE-END

#endif // CONFIGURATION_H
/*******************************************************************************
 End of File
*/
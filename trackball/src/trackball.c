#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_spi.h"
#include "trackball.h"

pointer_delta_t Trackball_PointerDelta;

#define BUFFER_SIZE 2

typedef enum {
    ModulePhase_PoweredUp,
    ModulePhase_ProcessMotion,
    ModulePhase_ProcessDeltaY,
    ModulePhase_ProcessDeltaX,
} module_phase_t;

module_phase_t modulePhase = ModulePhase_PoweredUp;

uint8_t txBufferPowerUpReset[] = {0xba, 0x5a};
uint8_t txBufferGetMotion[] = {0x02, 0x00};
uint8_t txBufferGetDeltaY[] = {0x03, 0x00};
uint8_t txBufferGetDeltaX[] = {0x04, 0x00};

uint8_t rxBuffer[BUFFER_SIZE];
spi_master_handle_t handle;
spi_transfer_t xfer = {0};

void tx(uint8_t *txBuff)
{
    GPIO_WritePinOutput(TRACKBALL_NCS_GPIO, TRACKBALL_NCS_PIN, 1);
    GPIO_WritePinOutput(TRACKBALL_NCS_GPIO, TRACKBALL_NCS_PIN, 0);
    xfer.txData = txBuff;
    SPI_MasterTransferNonBlocking(TRACKBALL_SPI_MASTER, &handle, &xfer);
}

void trackballUpdate(SPI_Type *base, spi_master_handle_t *masterHandle, status_t status, void *userData)
{
    switch (modulePhase) {
        case ModulePhase_PoweredUp:
            tx(txBufferGetMotion);
            modulePhase = ModulePhase_ProcessMotion;
            break;
        case ModulePhase_ProcessMotion: ;
            uint8_t motion = rxBuffer[1];
            bool isMoved = motion & (1<<7);
            if (isMoved) {
                tx(txBufferGetDeltaY);
                modulePhase = ModulePhase_ProcessDeltaY;
            } else {
                tx(txBufferGetMotion);
            }
            break;
        case ModulePhase_ProcessDeltaY: ;
            int8_t deltaY = (int8_t)rxBuffer[1];
            Trackball_PointerDelta.x += deltaY; // This is correct given the sensor orientation.
            tx(txBufferGetDeltaX);
            modulePhase = ModulePhase_ProcessDeltaX;
            break;
        case ModulePhase_ProcessDeltaX: ;
            int8_t deltaX = (int8_t)rxBuffer[1];
            Trackball_PointerDelta.y += deltaX; // This is correct given the sensor orientation.
            tx(txBufferGetMotion);
            modulePhase = ModulePhase_ProcessMotion;
            break;
    }
}

void Trackball_Init(void)
{
    CLOCK_EnableClock(TRACKBALL_SHTDWN_CLOCK);
    PORT_SetPinMux(TRACKBALL_SHTDWN_PORT, TRACKBALL_SHTDWN_PIN, kPORT_MuxAsGpio);
    GPIO_WritePinOutput(TRACKBALL_SHTDWN_GPIO, TRACKBALL_SHTDWN_PIN, 0);

    CLOCK_EnableClock(TRACKBALL_NCS_CLOCK);
    PORT_SetPinMux(TRACKBALL_NCS_PORT, TRACKBALL_NCS_PIN, kPORT_MuxAsGpio);

    CLOCK_EnableClock(TRACKBALL_MOSI_CLOCK);
    PORT_SetPinMux(TRACKBALL_MOSI_PORT, TRACKBALL_MOSI_PIN, kPORT_MuxAlt3);

    CLOCK_EnableClock(TRACKBALL_MISO_CLOCK);
    PORT_SetPinMux(TRACKBALL_MISO_PORT, TRACKBALL_MISO_PIN, kPORT_MuxAlt3);

    CLOCK_EnableClock(TRACKBALL_SCK_CLOCK);
    PORT_SetPinMux(TRACKBALL_SCK_PORT, TRACKBALL_SCK_PIN, kPORT_MuxAlt3);

    uint32_t srcFreq = 0;
    spi_master_config_t userConfig;
    SPI_MasterGetDefaultConfig(&userConfig);
    userConfig.polarity = kSPI_ClockPolarityActiveLow;
    userConfig.phase = kSPI_ClockPhaseSecondEdge;
    userConfig.baudRate_Bps = 100000U;
    srcFreq = CLOCK_GetFreq(TRACKBALL_SPI_MASTER_SOURCE_CLOCK);
    SPI_MasterInit(TRACKBALL_SPI_MASTER, &userConfig, srcFreq);
    SPI_MasterTransferCreateHandle(TRACKBALL_SPI_MASTER, &handle, trackballUpdate, NULL);
    xfer.rxData = rxBuffer;
    xfer.dataSize = BUFFER_SIZE;
    tx(txBufferPowerUpReset);
}
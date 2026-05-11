#include <doomgeneric_freertos.h>
#include <doomgeneric.h>
#include "vga_driver.h"

void DG_SleepMs(uint32_t ms)
{
    const TickType_t period = pdMS_TO_TICKS(ms);

    TickType_t lastwake = xTaskGetTickCount();

    vTaskDelayUntil(&lastwake, period);
}

uint32_t DG_GetTicksMs()
{
    uint32_t ticks = xTaskGetTickCount();

    return pdTICKS_TO_MS(ticks);
}

int DG_GetKey(int *pressed, unsigned char *key)
{
}

// initialize the vga card
void DG_Init() {
    //set to 640x400 mode
    *(volatile uint32_t*)VGA_CONTROL_REG = VGA_CTRL_MODE_640x400;
    
    //might be dumb, but enabling it too soon is bad
    //enable DMAIA mode
    *(volatile uint32_t*)VGA_IRQ_ENABLE_REG = VGA_IRQ_DMAEN;
}

// flip buffer switch, all the the pixels should have been written directly to buffer
void DG_DrawFrame()
{
    // I shouldn't need a semaphore since this is always called when 
    // rendering is done in the doom task
    if (xSemaphoreTake(dma_semaphore, portMAX_DELAY) == pdTRUE)
    {
        *(volatile uint32_t*)VGA_DMA_SRC_ADDR_REG = (uint32_t)DG_ScreenBuffer;
        *(volatile uint32_t*)VGA_DMA_LENGTH_REG = (uint32_t)FRAMEBUFFER_SIZE;
    }
}

// this should never be used
void DG_SetWindowTitle(const char *title) {
    //maybe print the window title
}
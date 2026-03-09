#include <doomgeneric_freertos.h>
#include <doomgeneric.h>

void DG_SleepMs(uint32_t ms)
{
    const TickType_t period = pdMS_TO_TICKS(ms);

    TickType_t lastwake = xTaskGetTickCount();

    vTaskDelayUntil(&lastwake, period);
}

uint32_t DG_GetTicksMs() {
    uint32_t ticks =  xTaskGetTickCount();

    return pdTICKS_TO_MS(ticks);
}

/*
    I'm thinking of using pipes to get these
*/
int DG_GetKey(int* pressed, unsigned char* key) {

}

void DG_Init() {}

//flip buffer switch, all the the pixels should have been written directly to buffer
void DG_DrawFrame() {}

//this should never be used
void DG_SetWindowTitle(const char * title) {}
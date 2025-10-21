#include "Headlight.h"
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#include "IfxPort.h"
#include "IfxStm.h"

#include "my_stdio.h"
#include "Evadc.h"

// (UDS 0x2E로 g_headlight_threshold 변경 가능)
volatile uint16_t g_headlight_threshold = 3500;

void HBA_Init ()
{
    // --- 💡 여기가 추가된 부분입니다 ---
    uint16 psw = IfxScuWdt_getCpuWatchdogPassword();
    IfxScuWdt_clearCpuEndinit(psw);
    MODULE_P02.IOCR4.B.PC4 = 0x10;
    IfxScuWdt_setCpuEndinit(psw);

    HBA_OFF();
}

void HBA_ON_ADC ()
{
    int adcResult = Evadc_readVR();

    if(adcResult > g_headlight_threshold) {
        MODULE_P02.OUT.B.P4 = 1;
    }
    else {
        MODULE_P02.OUT.B.P4 = 0;
    }
}

void HBA_ON ()
{
    MODULE_P02.OUT.B.P4 = 1;
}

void HBA_OFF ()
{
    MODULE_P02.OUT.B.P4 = 0;
}

// UDS(DoIP.c)에서 호출할 'Setter' 함수 추가
void set_headlight_threshold(uint16_t new_threshold)
{
    g_headlight_threshold = new_threshold;
}

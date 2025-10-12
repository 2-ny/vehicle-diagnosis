#include "Headlight.h"

// 현재 전조등 밝기를 저장할 전역 변수 (0 - 255 범위)
volatile uint8 g_headlight_brightness = 0;

void HBA_Init(){
    GtmAtomPwm_Init();
    // Set duty 0
    GtmAtomPwm_SetDutyCycle(0);
}

void HBA_ON(){
    int adcResult=Evadc_readPR();
    int duty = adcResult / 20;
    if(duty <= 100 && duty >= 0) {
        GtmAtomPwm_SetDutyCycle(duty);
    }
}

void HBA_OFF(){
    GtmAtomPwm_SetDutyCycle(0);
}

// 밝기 설정 함수 (UDS 0x2F 서비스가 호출할 함수)
void set_headlight_brightness(uint8 brightness)
{
    // 1. 전역 변수에 현재 밝기 상태를 저장 (0~255)
    g_headlight_brightness = brightness;

    // 2. 0~255 범위의 밝기 값을 0~100 범위의 PWM 듀티 사이클로 변환
    uint32 duty = (uint32)(((float)g_headlight_brightness / 255.0f) * 100.0f);
    if (duty > 100) duty = 100;

    // 3. PWM 듀티 사이클을 설정하여 실제 LED 밝기 조절
    GtmAtomPwm_SetDutyCycle(duty);
}

// 현재 밝기 조회 함수 (UDS 0x22 서비스가 호출할 함수)
uint8 get_headlight_brightness(void)
{
    return g_headlight_brightness;
}

// HBA_Init, HBA_ON, HBA_OFF 함수는 그대로 두거나, set_headlight_brightness 함수를 사용하도록 수정할 수 있습니다.
// 예를 들어 HBA_OFF는 set_headlight_brightness(0)을 호출하도록 변경할 수 있습니다.

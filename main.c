#include "main.h"

void main (void)
{
    // CPU 인터럽트 활성화 및 워치독 비활성화 (디버깅용)
    IfxCpu_enableInterrupts();
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    SYSTEM_Init();

    // 2. 이더넷(LwIP) 통신 스택 초기화
    eth_addr_t ethAddr = {.addr[0] = 0x00, .addr[1] = 0x00, .addr[2] = 0x0c, .addr[3] = 0x11, .addr[4] = 0x11, .addr[5
            ] = 0x11};
    initLwip(ethAddr); // LwIP 스택을 시작하고 IP 주소를 설정합니다.

    while (1)
    {
        unsigned int brightness = Evadc_readVR();
        //my_printf("조도 센서: %d\n", brightness);
        delay_ms(1000);
        // LwIP 타이머 처리 (ARP 타임아웃, TCP 재전송 등)
        Ifx_Lwip_pollTimerFlags();
        // 수신된 이더넷 패킷 처리
        Ifx_Lwip_pollReceiveFlags();
    } /* End of while */
}

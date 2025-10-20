#include "DTCManager.h"
#include "my_stdio.h"
#include <string.h> // memset을 사용하기 위해

volatile uint32 g_dtc_memory[10];
volatile uint8 g_dtc_count = 0;

// DTC 메모리 초기화
void DTCManager_Init (void)
{
    DTCManager_ClearFaults();
}

// 고장 기록 "쓰기" (내부 함수)
void DTCManager_ReportFault (uint32 dtc_code)
{
    // 1. 함수가 호출되었고, 어떤 DTC 코드를 받았는지 출력
    my_printf("[DTC LOG] ReportFault called with DTC: 0x%X\n", dtc_code);

    if (g_dtc_count >= 10)
    {
        my_printf("[DTC LOG] Memory is full. DTC not added.\n");
        return; // 메모리 꽉 참
    }

    // 이미 저장된 고장인지 확인
    for (int i = 0; i < g_dtc_count; i++)
    {
        if (g_dtc_memory[i] == dtc_code)
        {
            // 4. 이미 존재하는 DTC라서 추가하지 않는 경우 출력
            my_printf("[DTC LOG] DTC 0x%X already exists. Not adding again.\n", dtc_code);
            return;
        }
    }

    // 새 고장 코드 추가
    g_dtc_memory[g_dtc_count] = dtc_code;
    g_dtc_count++;

    // 6. 성공적으로 DTC를 추가했음을 출력
    my_printf("[DTC LOG] New DTC 0x%X added. Total count: %d\n", dtc_code, g_dtc_count);
}


// 고장 기록 "삭제"
void DTCManager_ClearFaults (void)
{
memset((void*) g_dtc_memory, 0, sizeof(g_dtc_memory));
g_dtc_count = 0;
}

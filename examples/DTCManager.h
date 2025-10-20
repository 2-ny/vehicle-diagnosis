#ifndef DTCMANAGER_H_
#define DTCMANAGER_H_

#include "Ifx_Types.h"
#include "stdbool.h" // bool, true, false 사용

/* --- DTC 정의 --- */
// UDS 표준에 따라 3바이트로 정의합니다.
#define DTC_TOF_FAULT 0x010001 // 예시: ToF 센서 신호 이상

/* --- 함수 선언 --- */
void DTCManager_Init(void);
void DTCManager_ReportFault(uint32 dtc_code);
void DTCManager_ClearFaults(void);

// DoIP.c에서 고장 목록을 읽어갈 수 있도록 전역 변수 선언
extern volatile uint32 g_dtc_memory[10]; // 최대 10개 DTC 저장
extern volatile uint8 g_dtc_count;

#endif /* DTCMANAGER_H_ */

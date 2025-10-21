#ifndef UDS_ON_CAN_H_
#define UDS_ON_CAN_H_

#include "Ifx_Types.h"

/* --- UDS 서비스 및 DID 정의 --- */
#define SID_READ_DATA_BY_ID     0x22
#define SID_WRITE_DATA_BY_ID    0x2E
#define SID_IO_CONTROL_BY_ID    0x2F

#define DID_LIGHT_SENSOR        0x0101  // 조도 센서 값 읽기
#define DID_HEADLIGHT           0x0300  // 전조등 제어
#define DID_HEADLIGHT_THRESHOLD 0x0301  // 전조등 임계값 쓰기/읽기

/* --- CAN ID 정의 (UDS 표준) --- */
#define UDS_CAN_ID_REQUEST      0x7E0   // 진단기가 ECU로 보내는 요청 ID
#define UDS_CAN_ID_RESPONSE     0x7E8   // ECU가 진단기로 보내는 응답 ID

/* --- 함수 프로토타입 --- */
void UdsOnCan_Init(void);

#endif /* UDS_ON_CAN_H_ */

#include "UdsOnCan.h"
#include "isotp.h"      // IsoTp_Send, IsoTp_RegisterCallback 사용
#include "evadc.h"      // Evadc_readPR() 사용
#include "Headlight.h"  // HBA_ON(), HBA_OFF() 사용

// ISO-TP로부터 완성된 UDS 메시지를 수신했을 때 호출될 콜백 함수
static void uds_on_can_callback (uint32 id, uint8 *data, uint16 len)
{
    if (len < 1)
        return;

    uint8 service_id = data[0]; // UDS 메시지의 첫 바이트는 SID

    switch (service_id)
    {
        case SID_READ_DATA_BY_ID : // 0x22
        {
            if (len < 3)
                break;
            uint16 data_id = (uint16) (data[1] << 8) | data[2];

            if (data_id == DID_LIGHT_SENSOR)
            {
                uint16 adc_val = (uint16) Evadc_readVR();

                // 긍정 응답: [SID+0x40] [DID] [값]
                uint8 response_data[] = {(SID_READ_DATA_BY_ID + 0x40), (uint8) (data_id >> 8), (uint8) (data_id),
                        (uint8) (adc_val >> 8), (uint8) (adc_val)};
                IsoTp_Send(UDS_CAN_ID_RESPONSE, response_data, sizeof(response_data));
            }
            else if (data_id == DID_HEADLIGHT_THRESHOLD)
            {
                // Headlight.c에 있는 함수를 호출하여 현재 임계값을 가져옵니다.
                uint16 threshold_val = get_headlight_threshold();

                // 긍정 응답 생성: [SID+0x40][DID][Value]
                uint8 response_data[] = {(SID_READ_DATA_BY_ID + 0x40), // 0x62
                        (uint8) (data_id >> 8), (uint8) (data_id), (uint8) (threshold_val >> 8),   // 임계값 상위 바이트
                        (uint8) (threshold_val)         // 임계값 하위 바이트
                        };
                IsoTp_Send(UDS_CAN_ID_RESPONSE, response_data, sizeof(response_data));
            }
            break;
        }

        case SID_IO_CONTROL_BY_ID : // 0x2F
        {
            if (len < 4)
                break;
            uint16 data_id = (uint16) (data[1] << 8) | data[2];

            if (data_id == DID_HEADLIGHT)
            {
                uint8 control_option = data[3];
                if (control_option == 0x03) // shortTermAdjustment
                {
                    uint8 control_state = data[4]; // On(1) or Off(0)
                    if (control_state == 1)
                    {
                        HBA_ON(); // 실제 하드웨어 제어 함수 호출
                    }
                    else
                    {
                        HBA_OFF();
                    }
                }

                // 긍정 응답: [SID+0x40] [DID]
                uint8 response_data[] = {(SID_IO_CONTROL_BY_ID + 0x40), (uint8) (data_id >> 8), (uint8) (data_id)};
                IsoTp_Send(UDS_CAN_ID_RESPONSE, response_data, sizeof(response_data));
            }
            break;
        }

        case SID_WRITE_DATA_BY_ID : // 0x2E
        {
            // UDS 요청 형식: [SID(1)][DID(2)][Value(2)] -> 최소 길이 5
            if (len < 5)
                break;
            uint16 data_id = (uint16) (data[1] << 8) | data[2];

            // DID가 전조등 임계값(0x0301)인지 확인
            if (data_id == DID_HEADLIGHT_THRESHOLD)
            {
                // UDS 메시지에서 2바이트의 새로운 임계값을 추출합니다.
                uint16 new_threshold = (uint16) (data[3] << 8) | data[4];

                // Headlight.c에 있는 함수를 호출하여 값을 설정합니다.
                set_headlight_threshold(new_threshold);

                // 긍정 응답 생성: [SID+0x40][DID]
                uint8 response_data[] = {(SID_WRITE_DATA_BY_ID + 0x40), // 0x6E
                        (uint8) (data_id >> 8), (uint8) (data_id)};
                IsoTp_Send(UDS_CAN_ID_RESPONSE, response_data, sizeof(response_data));
            }
            break;
        }

        default :
            // 지원하지 않는 서비스에 대한 부정 응답(NRC) 처리 (필요 시 구현)
            break;
    }
}

// UDS on CAN 모듈 초기화
void UdsOnCan_Init (void)
{
    IsoTp_Init(); // ISO-TP 모듈 초기화
    IsoTp_RegisterCallback(uds_on_can_callback); // 콜백 함수 등록
}

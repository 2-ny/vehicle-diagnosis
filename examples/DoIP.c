#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "string.h"
#include "stdbool.h"
#include "DoIP.h"
#include "ToF.h"
#include "Motor.h"
#include "DTCManager.h"
#include "stdbool.h"

/* --- UDS 세션 상태 정의 --- */
typedef enum
{
    DEFAULT_SESSION = 0x01, EXTENDED_SESSION = 0x03
} DiagnosticSessionState;

// 현재 세션 상태를 저장할 전역 변수 (부팅 시 기본 세션으로 시작)
volatile DiagnosticSessionState g_current_session_state = DEFAULT_SESSION;

/* --- UDS 서비스 및 DID 정의 --- */
#define SID_DIAGNOSTIC_SESSION_CONTROL  0x10    // 세션 제어
#define SID_TESTER_PRESENT              0x3E    // 연결 유지
#define SID_READ_DATA_BY_ID     0x22
#define SID_IO_CONTROL_BY_ID    0x2F
#define SID_READ_DTC_INFO       0x19
#define SID_CLEAR_DTC_INFO      0x14

#define DID_TOF                 0x1001
#define DID_MOTOR_CONTROL       0x4000

#if LWIP_TCP

/* --- 함수 프로토타입 선언 --- */
static err_t doip_accept (void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t doip_recv (void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void doip_error (void *arg, err_t err);

#define UDS_PORT 13400

void DoIP_Init (void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (pcb != NULL)
    {
        err_t err = tcp_bind(pcb, IP_ADDR_ANY, UDS_PORT);
        if (err == ERR_OK)
        {
            pcb = tcp_listen(pcb);
            tcp_accept(pcb, doip_accept);
        }
        else
        {
            tcp_abort(pcb);
        }
    }
}

static err_t doip_accept (void *arg, struct tcp_pcb *newpcb, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);
    tcp_recv(newpcb, doip_recv);
    tcp_err(newpcb, doip_error);
    return ERR_OK;
}

static err_t doip_recv (void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (p == NULL)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }
    if (err != ERR_OK)
    {
        pbuf_free(p);
        return err;
    }

    uint16_t payload_type = (uint16_t) (((uint8_t*) p->payload)[2] << 8) | ((uint8_t*) p->payload)[3];

    if (payload_type == 0x0005) // Routing Activation Request
    {
        uint8_t resp[] = {0x03, 0xFC, 0x00, 0x06, 0x00, 0x00, 0x00, 0x09, ((uint8_t*) p->payload)[8],
                ((uint8_t*) p->payload)[9], 0x02, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00};
        tcp_write(tpcb, resp, sizeof(resp), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    }
    else if (payload_type == 0x8001) // Diagnostic Message
    {
        uint8_t service_id = ((uint8_t*) p->payload)[12];
        uint16_t data_id = (uint16_t) (((uint8_t*) p->payload)[13] << 8) | ((uint8_t*) p->payload)[14];

        // --- 💡 service_id 를 기준으로 switch 문 사용 ---
        switch (service_id)
        {
            // --- 💡 1. 세션 제어 (0x10) 로직 추가 ---
            case SID_DIAGNOSTIC_SESSION_CONTROL : // 0x10
            {
                uint8_t sub_function = ((uint8_t*) p->payload)[13]; // 서브 기능 (세션 종류)
                bool success = false;

                if (sub_function == DEFAULT_SESSION || sub_function == EXTENDED_SESSION)
                {
                    g_current_session_state = (DiagnosticSessionState) sub_function;
                    success = true;
                }

                if (success)
                {
                    // 긍정 응답 (0x50)
                    uint8_t resp[] = {0x03, 0xFC, 0x80, 0x01, 0x00, 0x00, 0x00, 0x06, // DoIP Header
                            ((uint8_t*) p->payload)[10], ((uint8_t*) p->payload)[11], // TA
                            ((uint8_t*) p->payload)[8], ((uint8_t*) p->payload)[9],   // SA
                            0x50, sub_function // UDS (SID 0x50 + SubFn)
                            };
                    tcp_write(tpcb, resp, sizeof(resp), TCP_WRITE_FLAG_COPY);
                }
                // (필요 시) else: 부정 응답(NRC) 전송

                tcp_output(tpcb);
                break;
            }

                // --- 💡 2. 연결 유지 (0x3E) 로직 추가 ---
            case SID_TESTER_PRESENT : // 0x3E
            {
                uint8_t sub_function = ((uint8_t*) p->payload)[13];
                if (sub_function == 0x00) // zeroSubFunction
                {
                    // (세션 타임아웃 타이머 리셋 로직이 있다면 여기에 추가)

                    // 긍정 응답 (0x7E)
                    uint8_t resp[] = {0x03, 0xFC, 0x80, 0x01, 0x00, 0x00, 0x00, 0x06, ((uint8_t*) p->payload)[10],
                            ((uint8_t*) p->payload)[11], ((uint8_t*) p->payload)[8], ((uint8_t*) p->payload)[9], 0x7E,
                            sub_function // UDS (SID 0x7E + SubFn)
                            };
                    tcp_write(tpcb, resp, sizeof(resp), TCP_WRITE_FLAG_COPY);
                    tcp_output(tpcb);
                }
                break;
            }
            case SID_READ_DATA_BY_ID : // 0x22 ReadDataByIdentifier
            {
                if (data_id == DID_TOF)
                {
                    uint16_t distance = Tof_GetValue();
                    uint8_t resp[] = {0x03, 0xFC, 0x80, 0x01, 0x00, 0x00, 0x00, 0x09, // DoIP Header
                            ((uint8_t*) p->payload)[10], ((uint8_t*) p->payload)[11], // Target Address (요청의 SA를 복사)
                            ((uint8_t*) p->payload)[8], ((uint8_t*) p->payload)[9],   // Source Address (요청의 TA를 복사)
                            0x62,                         // UDS Positive Response SID
                            (uint8_t) (data_id >> 8),      // DID High
                            (uint8_t) (data_id),           // DID Low
                            (uint8_t) (distance >> 8),     // 거리 값 High
                            (uint8_t) (distance)           // 거리 값 Low
                            };
                    tcp_write(tpcb, resp, sizeof(resp), TCP_WRITE_FLAG_COPY);
                    tcp_output(tpcb);
                }
                // 여기에 다른 DID_READ 처리 case 추가 가능
                break;// case 0x22 종료
            }

                // --- 💡 여기가 새로 추가된 부분입니다: 0x2F 처리 ---
            case SID_IO_CONTROL_BY_ID : // 0x2F InputOutputControlByIdentifier
            {
                // 💡 중요: 강제 제어는 '확장 세션'에서만 허용
                if (g_current_session_state != EXTENDED_SESSION)
                {
                    // 부정 응답 (NRC 0x7F - serviceNotSupportedInActiveSession)
                    // ... (NRC 응답 전송 로직) ...
                    break;
                }

                if (data_id == DID_MOTOR_CONTROL) // 모터 제어 요청인지 확인
                {
                    uint8_t control_option = ((uint8_t*) p->payload)[15];
                    bool success = true; // 응답 성공 여부 플래그

                    switch (control_option)
                    {
                        case 0x03 : // shortTermAdjustment (강제 구동/제동)
                        {
                            // UDS 페이로드에서 2바이트의 controlState (방향, 속도)를 읽어옵니다.
                            uint8_t new_direction = ((uint8_t*) p->payload)[16]; // 방향 (0 또는 1)
                            uint8_t new_speed = ((uint8_t*) p->payload)[17]; // 속도 (0 ~ 100)

                            // Motor.c의 함수를 호출하여 모터를 강제 구동/제동합니다.
                            Motor_movChB_PWM(new_speed, new_direction);
                            break;
                        }
                        case 0x00 : // returnControlToECU (제어권 반환)
                        {
                            // 제어권을 반환할 때는 모터를 정지시킵니다.
                            Motor_stopChB();
                            break;
                        }
                        default :
                            success = false; // 지원하지 않는 옵션
                            break;
                    }

                    if (success)
                    {
                        // 긍정 응답 (0x6F) 메시지 생성
                        uint8_t resp[] = {0x03, 0xFC, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07, // DoIP Header
                                ((uint8_t*) p->payload)[10], ((uint8_t*) p->payload)[11], // Target Address
                                ((uint8_t*) p->payload)[8], ((uint8_t*) p->payload)[9],   // Source Address
                                0x6F, (uint8_t) (data_id >> 8), (uint8_t) (data_id) // UDS Payload (SID 0x6F + DID)
                                };
                        tcp_write(tpcb, resp, sizeof(resp), TCP_WRITE_FLAG_COPY);
                        tcp_output(tpcb);
                    }
                    else
                    {
                        // (필요 시) 지원하지 않는 옵션에 대한 부정 응답(NRC) 전송
                    }
                }
                else
                {
                    // 지원하지 않는 DID -> NRC 전송 로직
                }
                break; // case 0x2F 종료
            }
                // --- 💡 추가된 부분 끝 ---

                //--- 💡 여기가 DTC 읽기(0x19) 처리 로직입니다 ---
            case SID_READ_DTC_INFO : // 0x19
            {
                // UDS 페이로드에서 Sub-function (reportType)을 읽습니다.
                uint8_t report_type = ((uint8_t*) p->payload)[13];

                // 'reportDTCByStatusMask (0x02)' 요청만 처리하도록 단순화
                if (report_type == 0x02)
                {
                    // 응답 메시지 길이는 DTC 개수에 따라 가변적입니다.
                    uint8_t dtc_count = g_dtc_count;
                    uint16_t uds_payload_len = 2 + (dtc_count * 4); // UDS 헤더(2) + DTC당 4바이트
                    uint16_t doip_payload_len = 4 + uds_payload_len; // SA/TA(4) + UDS 페이로드
                    uint16_t total_len = 8 + doip_payload_len; // DoIP 헤더(8) + DoIP 페이로드

                    // 가변 길이 응답을 위한 동적 할당 (또는 충분히 큰 배열)
                    uint8_t resp[total_len];

                    // DoIP 헤더 (0x8001)
                    resp[0] = 0x03;
                    resp[1] = 0xFC;
                    resp[2] = 0x80;
                    resp[3] = 0x01;
                    resp[4] = (uint8_t) (doip_payload_len >> 8);
                    resp[5] = (uint8_t) (doip_payload_len);
                    resp[6] = 0x00;
                    resp[7] = 0x00;

                    // DoIP 페이로드 (SA/TA)
                    resp[8] = ((uint8_t*) p->payload)[10];
                    resp[9] = ((uint8_t*) p->payload)[11]; // TA
                    resp[10] = ((uint8_t*) p->payload)[8];
                    resp[11] = ((uint8_t*) p->payload)[9]; // SA

                    // UDS 응답 (0x59)
                    resp[12] = 0x59; // 0x19 + 0x40
                    resp[13] = report_type; // 요청한 Sub-function
                    resp[14] = 0x29; // DTCStatusAvailabilityMask (예: "testFailed" 비트만 활성화)

                    // DTC 목록 (DTC 3바이트 + Status 1바이트)
                    for (int i = 0; i < dtc_count; i++)
                    {
                        uint32_t dtc = g_dtc_memory[i];
                        resp[15 + (i * 4)] = (uint8_t) (dtc >> 16); // DTC HighByte
                        resp[16 + (i * 4)] = (uint8_t) (dtc >> 8);  // DTC MiddleByte
                        resp[17 + (i * 4)] = (uint8_t) (dtc);       // DTC LowByte
                        resp[18 + (i * 4)] = 0x01; // DTC Status (예: testFailed)
                    }

                    tcp_write(tpcb, resp, total_len, TCP_WRITE_FLAG_COPY);
                    tcp_output(tpcb);
                }
                break;
            }
                // --- 💡 여기가 DTC 삭제(0x14) 처리 로직입니다 ---
            case SID_CLEAR_DTC_INFO : // 0x14
            {
                DTCManager_ClearFaults(); // DTC 메모리 전체 삭제

                // 긍정 응답 (0x54) 전송
                uint8_t resp[] = {0x03, 0xFC, 0x80, 0x01, 0x00, 0x00, 0x00, 0x04, ((uint8_t*) p->payload)[10],
                        ((uint8_t*) p->payload)[11], ((uint8_t*) p->payload)[8], ((uint8_t*) p->payload)[9], 0x54 // 0x14 + 0x40
                        };
                tcp_write(tpcb, resp, sizeof(resp), TCP_WRITE_FLAG_COPY);
                tcp_output(tpcb);
                break;
            }
                // --- 💡 추가된 부분 끝 ---
            default :
                // 지원하지 않는 서비스 ID -> NRC 전송 로직
                break;
        } // switch (service_id) 종료
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

static void doip_error (void *arg, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);
}

#endif /* LWIP_TCP */

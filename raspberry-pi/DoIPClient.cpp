#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <iomanip> // for std::hex
#include <stdexcept> // for std::runtime_error

// Platform-specific socket headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cstring>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket(s) close(s)
#endif

// --- DID 및 SID 정의 ---
#define SID_READ_DATA_BY_ID     0x22
#define SID_IO_CONTROL_BY_ID    0x2F
#define DID_TOF                 0x1001
#define DID_MOTOR_CONTROL       0x4000

// Helper function to print a byte vector as a hex string
void print_hex(const std::string& prefix, const std::vector<uint8_t>& data) {
    std::cout << prefix;
    std::ios_base::fmtflags f(std::cout.flags());
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout.flags(f);
    std::cout << std::dec << std::endl; // 10진수로 복구
}

// --- 💡 여기가 수정된 부분입니다: UDS 파싱 인덱스 수정 ---
void request_and_parse_did(SOCKET sock, uint16_t did, const std::string& did_name, int value_bytes) {
    std::cout << "\n--- [" << did_name << "] 값 요청 시작 ---" << std::endl;
    
    const uint32_t payload_len = 7; // SA(2) + TA(2) + UDS(3) = 7
    
    std::vector<uint8_t> request_msg = {
        0x03, 0xFC,                             // Protocol Version
        0x80, 0x01,                             // Payload Type
        (uint8_t)(payload_len >> 24),           // Length
        (uint8_t)(payload_len >> 16),
        (uint8_t)(payload_len >> 8),
        (uint8_t)(payload_len),
        0x0E, 0x80,                             // Source Address
        0x02, 0x01,                             // Target Address
        SID_READ_DATA_BY_ID,                    // UDS SID (0x22)
        (uint8_t)(did >> 8),                    // DID High
        (uint8_t)(did & 0xFF)                   // DID Low
    };
    
    print_hex("요청 전송: ", request_msg);
    if (send(sock, (const char*)request_msg.data(), request_msg.size(), 0) == SOCKET_ERROR) {
        throw std::runtime_error(did_name + " 요청 실패.");
    }

    std::vector<uint8_t> response_buf(1024);
    int bytes_received = recv(sock, (char*)response_buf.data(), response_buf.size(), 0);

    if (bytes_received <= 0) throw std::runtime_error(did_name + " 응답 수신 실패.");
    
    response_buf.resize(bytes_received);
    print_hex("응답 수신: ", response_buf);
    
    // 💡 수정된 파싱 로직:
    // DoIP 헤더(8) + TA(2) + SA(2) = 12바이트. SID는 인덱스 12부터 시작
    size_t expected_size = (value_bytes == 2) ? 17 : 16;
    if (response_buf.size() >= expected_size && response_buf[12] == 0x62) { // 0x62 SID 확인
        uint16_t resp_did = (static_cast<uint16_t>(response_buf[13]) << 8) | response_buf[14]; // DID는 인덱스 13, 14
        if (resp_did == did) {
            if (value_bytes == 2) {
                uint16_t value = (static_cast<uint16_t>(response_buf[15]) << 8) | response_buf[16]; // 값은 인덱스 15, 16
                std::cout << "✅ 수신된 값 (" << did_name << "): " << value << std::endl;
            } else { // 1 byte
                uint8_t value = response_buf[15]; // 값은 인덱스 15
                std::cout << "✅ 수신된 값 (" << did_name << "): " << static_cast<int>(value) << std::endl;
            }
        } else {
            std::cout << "❌ UDS 응답 오류 (DID 불일치)" << std::endl;
        }
    } else {
        std::cout << "❌ UDS 응답 형식이 올바르지 않습니다." << std::endl;
    }
    std::cout << "--- [" << did_name << "] 값 요청 종료 ---" << std::endl;
}


int main() {
    const char* TC375_IP = "192.168.10.20"; 
    const int DOIP_PORT = 13400;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { std::cerr << "WSAStartup failed." << std::endl; return 1; }
#endif

    SOCKET client_socket = INVALID_SOCKET;
    try {
        // 1 & 2. 소켓 생성 및 TCP 연결
        client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client_socket == INVALID_SOCKET) throw std::runtime_error("소켓 생성 실패.");
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(DOIP_PORT);
        inet_pton(AF_INET, TC375_IP, &server_addr.sin_addr);

        std::cout << "TC375 (" << TC375_IP << ":" << DOIP_PORT << ")에 연결을 시도합니다..." << std::endl;
        if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            throw std::runtime_error("TCP 연결 실패.");
        }
        std::cout << "✅ TCP 연결 성공!" << std::endl;

        // 3 & 4. 라우팅 활성화 및 응답 확인
        std::vector<uint8_t> route_request_msg = {
            0x03, 0xFC, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07, 
            0x0E, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        print_hex("라우팅 활성화 요청 전송: ", route_request_msg);
        send(client_socket, (const char*)route_request_msg.data(), route_request_msg.size(), 0);
        
        std::vector<uint8_t> response_buf(1024);
        int bytes_received = recv(client_socket, (char*)response_buf.data(), response_buf.size(), 0);
        if (bytes_received <= 0) throw std::runtime_error("라우팅 활성화 응답 수신 실패.");

        response_buf.resize(bytes_received);
        print_hex("라우팅 활성화 응답 수신: ", response_buf);
        
        if (response_buf.size() >= 13 && response_buf[2] == 0x00 && response_buf[3] == 0x06 && response_buf[12] == 0x10) {
            std::cout << "✅ 라우팅 활성화 성공!" << std::endl;
            
            // --- 1. ToF 센서 값 읽기 ---
            usleep(100 * 1000); // 0.1초 대기
            request_and_parse_did(client_socket, DID_TOF, "ToF 센서", 2);

            // --- 2. 모터 강제 구동 (PWM: 100, Direction: 1) ---
            std::cout << "\n--- [모터 강제 구동] 요청 시작 ---" << std::endl;
            const uint8_t motor_dir = 1;
            const uint8_t motor_speed = 100;
            const uint32_t motor_payload_len = 10; // SA(2)+TA(2)+UDS(6) = 10
            
            std::vector<uint8_t> motor_req_msg = {
                0x03, 0xFC,
                0x80, 0x01,
                (uint8_t)(motor_payload_len >> 24), (uint8_t)(motor_payload_len >> 16),
                (uint8_t)(motor_payload_len >> 8),  (uint8_t)(motor_payload_len),
                0x0E, 0x80, 0x02, 0x01, // SA, TA
                SID_IO_CONTROL_BY_ID,   // SID (0x2F)
                (uint8_t)(DID_MOTOR_CONTROL >> 8), (uint8_t)(DID_MOTOR_CONTROL), // DID
                0x03,       // Control Option (shortTermAdjustment)
                motor_dir,  // Control State (Direction)
                motor_speed // Control State (Speed)
            };
            
            print_hex("요청 전송: ", motor_req_msg);
            send(client_socket, (const char*)motor_req_msg.data(), motor_req_msg.size(), 0);

            bytes_received = recv(client_socket, (char*)response_buf.data(), response_buf.size(), 0);
            if (bytes_received <= 0) throw std::runtime_error("모터 제어 응답 수신 실패.");

            response_buf.resize(bytes_received);
            print_hex("응답 수신: ", response_buf);

            // 💡 수정된 부분: 응답 길이는 15바이트 (Header 8 + SA/TA 4 + UDS 3)
            if (response_buf.size() >= 15 && response_buf[12] == 0x6F) {
                uint16_t resp_did = (static_cast<uint16_t>(response_buf[13]) << 8) | response_buf[14];
                if (resp_did == DID_MOTOR_CONTROL) {
                    std::cout << "✅ 모터 강제 구동 요청 성공!" << std::endl;

                    // --- 💡 3. 5초 대기 ---
                    std::cout << "--- 5초간 구동합니다... ---" << std::endl;
                    usleep(5000 * 1000); // 5,000,000 microseconds = 5 seconds

                    // --- 💡 4. 모터 강제 제동 (PWM: 0, Direction: 0) ---
                    std::cout << "\n--- [모터 강제 제동] 요청 시작 ---" << std::endl;
                    const uint8_t brake_dir = 0;
                    const uint8_t brake_speed = 0;
                    
                    std::vector<uint8_t> brake_req_msg = {
                        0x03, 0xFC,
                        0x80, 0x01,
                        (uint8_t)(motor_payload_len >> 24), (uint8_t)(motor_payload_len >> 16),
                        (uint8_t)(motor_payload_len >> 8),  (uint8_t)(motor_payload_len),
                        0x0E, 0x80, 0x02, 0x01,
                        SID_IO_CONTROL_BY_ID,
                        (uint8_t)(DID_MOTOR_CONTROL >> 8), (uint8_t)(DID_MOTOR_CONTROL),
                        0x03,       // Control Option (shortTermAdjustment)
                        brake_dir,  // Control State (Direction 0)
                        brake_speed // Control State (Speed 0)
                    };
                    
                    print_hex("요청 전송: ", brake_req_msg);
                    send(client_socket, (const char*)brake_req_msg.data(), brake_req_msg.size(), 0);

                    bytes_received = recv(client_socket, (char*)response_buf.data(), response_buf.size(), 0);
                    if (bytes_received <= 0) throw std::runtime_error("모터 제동 응답 수신 실패.");

                    response_buf.resize(bytes_received);
                    print_hex("응답 수신: ", response_buf);

                    if (response_buf.size() >= 15 && response_buf[12] == 0x6F) {
                        resp_did = (static_cast<uint16_t>(response_buf[13]) << 8) | response_buf[14];
                        if (resp_did == DID_MOTOR_CONTROL) {
                            std::cout << "✅ 모터 강제 제동 요청 성공!" << std::endl;
                        }
                    } else {
                        std::cout << "❌ 모터 제동 요청 실패." << std::endl;
                    }
                }
            } else {
                std::cout << "❌ 모터 제어 요청 실패." << std::endl;
            }
            std::cout << "--- 테스트 종료 ---" << std::endl;

        } else {
            std::cout << "❌ 라우팅 활성화 실패." << std::endl;
        }
        
    } catch (const std::runtime_error& e) {
        std::cerr << "❌ 오류 발생: " << e.what() << std::endl;
    }

    if (client_socket != INVALID_SOCKET) {
        closesocket(client_socket);
        std::cout << "\n소켓을 닫았습니다." << std::endl;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
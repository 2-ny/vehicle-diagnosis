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
    #pragma comment(lib, "ws2_32.lib") // Link with ws2_32.lib
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cstring> // for strcpy
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket(s) close(s)
#endif

// Helper function to print a byte vector as a hex string
void print_hex(const std::string& prefix, const std::vector<uint8_t>& data) {
    std::cout << prefix;
    std::ios_base::fmtflags f(std::cout.flags());
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout.flags(f);
    std::cout << std::endl;
}

int main() {
    // --- 설정 ---
    const char* TC375_IP = "192.168.10.20"; 
    const int DOIP_PORT = 13400;
    // const char* INTERFACE_NAME = "eth0"; // Linux에서 특정 인터페이스를 강제할 때 사용

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "❌ WSAStartup failed." << std::endl;
        return 1;
    }
#endif

    SOCKET client_socket = INVALID_SOCKET;
    try {
        // 1. TCP 소켓 생성
        client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client_socket == INVALID_SOCKET) {
            throw std::runtime_error("소켓 생성 실패.");
        }
        std::cout << "1. 소켓 생성 완료." << std::endl;

        // 2. TC375 ECU에 연결 시도
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(DOIP_PORT);
        inet_pton(AF_INET, TC375_IP, &server_addr.sin_addr);

        std::cout << "2. TC375 (" << TC375_IP << ":" << DOIP_PORT << ")에 연결을 시도합니다..." << std::endl;
        if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            throw std::runtime_error("TCP 연결 실패.");
        }
        std::cout << "✅ TCP 연결 성공!" << std::endl;

        // 3. Routing Activation Request 메시지 생성 및 전송
        std::vector<uint8_t> route_request_msg = {
            0x02, 0xFD, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07, // DoIP Header
            0x0E, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00        // Payload
        };
        print_hex("3. 라우팅 활성화 요청 전송: ", route_request_msg);
        if (send(client_socket, (const char*)route_request_msg.data(), route_request_msg.size(), 0) == SOCKET_ERROR) {
            throw std::runtime_error("라우팅 활성화 요청 실패.");
        }
        
        // 4. 라우팅 활성화 응답 수신
        std::vector<uint8_t> response_buf(1024);
        int bytes_received = recv(client_socket, (char*)response_buf.data(), response_buf.size(), 0);
        
        if (bytes_received > 0) {
            response_buf.resize(bytes_received);
            print_hex("4. 라우팅 활성화 응답 수신: ", response_buf);
            
            uint16_t res_payload_type = (static_cast<uint16_t>(response_buf[2]) << 8) | response_buf[3];
            uint8_t response_code = response_buf[12];

            if (res_payload_type == 0x0006 && response_code == 0x10) {
                std::cout << "✅ 라우팅 활성화 성공! 이제 진단 메시지를 보낼 수 있습니다." << std::endl;
                
                // =======================================================================
                // --- 💡 여기가 새로 추가된 부분입니다: UDS 진단 요청 ---
                // =======================================================================

                // 5. UDS 진단 요청 메시지 생성 (ReadDataByIdentifier, DID 0x0002)
                std::vector<uint8_t> diag_request_msg = {
                    0x02, 0xFD, 0x80, 0x01, 0x00, 0x00, 0x00, 0x03, // DoIP Header (Payload Type 0x8001, Length 3)
                    0x22, 0x00, 0x02                                // UDS Payload (SID 0x22, DID 0x0002)
                };
                print_hex("\n5. 조도 센서 값 요청 전송 (UDS: 22 00 02): ", diag_request_msg);
                if (send(client_socket, (const char*)diag_request_msg.data(), diag_request_msg.size(), 0) == SOCKET_ERROR) {
                    throw std::runtime_error("진단 메시지 전송 실패.");
                }

                // 6. UDS 진단 응답 수신
                bytes_received = recv(client_socket, (char*)response_buf.data(), response_buf.size(), 0);
                if (bytes_received > 0) {
                    response_buf.resize(bytes_received);
                    print_hex("6. 조도 센서 값 응답 수신: ", response_buf);
                    
                    // 7. 응답 파싱 및 결과 출력
                    if (response_buf.size() >= 13) {
                        uint8_t uds_sid = response_buf[8];
                        uint16_t uds_did = (static_cast<uint16_t>(response_buf[9]) << 8) | response_buf[10];

                        if (uds_sid == 0x62 && uds_did == 0x0002) {
                            uint16_t adc_value = (static_cast<uint16_t>(response_buf[11]) << 8) | response_buf[12];
                            float voltage = static_cast<float>(adc_value) * 3.3f / 4095.0f;
                            
                            std::cout << "\n--- 최종 결과 ---" << std::endl;
                            std::cout << "✅ 조도 센서 ADC 값: " << adc_value << std::endl;
                            std::cout << "✅ 변환된 전압 값: " << std::fixed << std::setprecision(2) << voltage << " V" << std::endl;
                            std::cout << "-----------------" << std::endl;
                        } else {
                            std::cout << "❌ UDS 응답 오류 (SID 또는 DID 불일치)" << std::endl;
                        }
                    } else {
                        std::cout << "❌ UDS 응답 길이가 너무 짧습니다." << std::endl;
                    }
                } else {
                    throw std::runtime_error("진단 응답 수신 실패.");
                }
                // --- 💡 추가된 부분 끝 ---
                
            } else {
                std::cout << "❌ 라우팅 활성화 실패. 응답 코드: 0x" << std::hex << static_cast<int>(response_code) << std::endl;
            }
        }
        
    } catch (const std::runtime_error& e) {
#ifdef _WIN32
        std::cerr << "❌ 오류 발생: " << e.what() << " (에러 코드: " << WSAGetLastError() << ")" << std::endl;
#else
        perror(e.what());
#endif
    }

    // 소켓 닫기 및 정리
    if (client_socket != INVALID_SOCKET) {
        closesocket(client_socket);
        std::cout << "\n소켓을 닫았습니다." << std::endl;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
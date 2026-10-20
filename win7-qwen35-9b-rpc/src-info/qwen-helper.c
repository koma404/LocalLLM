// qwen-helper: small Win7-compatible helper for the batch scripts (winsock only)
//   qwen-helper myip                         print this PC's LAN IPv4
//   qwen-helper probe HOST:PORT [ms]         exit 0 if a TCP connect succeeds
//   qwen-helper plan WORKERS_DIR             probe registered workers, print "set" lines for main
//   qwen-helper health HOST:PORT [ms]        exit 0 if GET /health answers with "ok"
// build: x86_64-w64-mingw32-gcc -O2 -D_WIN32_WINNT=0x0601 qwen-helper.c -o qwen-helper.exe -lws2_32 -static
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// approximate weights of Qwen3.5-9B Q4_K_M with -ngl 99 (MiB): all RPC devices, and the output layer
#define RPC_TOTAL_MIB 4861.0
#define OUTPUT_MIB     700.0

static int split_hostport(const char * s, char * host, size_t hn, char * port, size_t pn) {
    const char * c = strrchr(s, ':');
    if (!c || (size_t)(c - s) >= hn || strlen(c + 1) >= pn) return 0;
    memcpy(host, s, c - s); host[c - s] = 0;
    strcpy(port, c + 1);
    return 1;
}

static SOCKET connect_to(const char * hostport, int ms) {
    char host[256], port[16];
    if (!split_hostport(hostport, host, sizeof(host), port, sizeof(port))) return INVALID_SOCKET;
    struct addrinfo hints = {0}, * res = NULL;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0 || !res) return INVALID_SOCKET;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
    connect(s, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    fd_set w, e; FD_ZERO(&w); FD_ZERO(&e); FD_SET(s, &w); FD_SET(s, &e);
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    if (select(0, NULL, &w, &e, &tv) != 1 || FD_ISSET(s, &e)) { closesocket(s); return INVALID_SOCKET; }
    nb = 0; ioctlsocket(s, FIONBIO, &nb);
    return s;
}

static int cmd_myip(void) {
    // the UDP "connect" picks the interface of the default route without sending anything
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(53);
    a.sin_addr.s_addr = inet_addr("8.8.8.8");
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == 0) {
        struct sockaddr_in me; int len = sizeof(me);
        if (getsockname(s, (struct sockaddr *)&me, &len) == 0 && me.sin_addr.s_addr != 0) {
            printf("%s\n", inet_ntoa(me.sin_addr)); closesocket(s); return 0;
        }
    }
    closesocket(s);
    // fallback: first non-loopback address of this host name
    char name[256];
    if (gethostname(name, sizeof(name)) == 0) {
        struct hostent * h = gethostbyname(name);
        for (int i = 0; h && h->h_addr_list[i]; i++) {
            struct in_addr ia; memcpy(&ia, h->h_addr_list[i], sizeof(ia));
            if ((ntohl(ia.s_addr) >> 24) != 127) { printf("%s\n", inet_ntoa(ia)); return 0; }
        }
    }
    return 1;
}

static int cmd_probe(const char * hp, int ms) {
    SOCKET s = connect_to(hp, ms);
    if (s == INVALID_SOCKET) return 1;
    closesocket(s); return 0;
}

static int cmd_health(const char * hp, int ms) {
    SOCKET s = connect_to(hp, ms);
    if (s == INVALID_SOCKET) return 1;
    DWORD to = ms; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof(to));
    const char * req = "GET /health HTTP/1.0\r\nHost: x\r\n\r\n";
    send(s, req, (int)strlen(req), 0);
    char buf[2048]; int n = 0, r;
    while (n < (int)sizeof(buf) - 1 && (r = recv(s, buf + n, sizeof(buf) - 1 - n, 0)) > 0) n += r;
    buf[n] = 0; closesocket(s);
    return (strstr(buf, " 200 ") && strstr(buf, "\"ok\"")) ? 0 : 1;
}

static int cmp_str(const void * a, const void * b) { return strcmp((const char *)a, (const char *)b); }

static int cmd_plan(const char * dir) {
    static char found[64][128]; int nf = 0;
    char pattern[MAX_PATH]; snprintf(pattern, sizeof(pattern), "%s\\*.txt", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char path[MAX_PATH]; snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
            FILE * f = fopen(path, "r"); if (!f) continue;
            char line[128] = {0};
            if (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\r\n \t")] = 0;
                if (line[0] && nf < 64) strcpy(found[nf++], line);
            }
            fclose(f);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    qsort(found, nf, sizeof(found[0]), cmp_str);
    char alive[64][128]; int na = 0;
    for (int i = 0; i < nf; i++) {
        int dup = 0;
        for (int j = 0; j < na; j++) if (!strcmp(alive[j], found[i])) dup = 1;
        if (dup) continue;
        int ok = cmd_probe(found[i], 1500) == 0;
        fprintf(stderr, "  worker %-24s %s\n", found[i], ok ? "OK" : "not reachable (skipped)");
        if (ok) strcpy(alive[na++], found[i]);
    }
    printf("set N_WORKERS=%d\n", na);
    if (na < 2) return 0;
    printf("set WORKERS=");
    for (int i = 0; i < na; i++) printf("%s%s", i ? "," : "", alive[i]);
    printf("\n");
    if (na == 2) {
        // 3 PCs: the main PC also keeps 10 of the 32 layers
        printf("set NGL=23\nset TS=14,9\n");
    } else {
        // equal share per worker; the last one also holds the output layer, so it gets fewer layers
        double t = RPC_TOTAL_MIB / na;
        int last = (int)(12.0 * (t - OUTPUT_MIB) / t + 0.5);
        if (last < 1) last = 1;
        printf("set NGL=99\nset TS=");
        for (int i = 0; i < na; i++) printf("%s%d", i ? "," : "", i == na - 1 ? last : 12);
        printf("\n");
    }
    return 0;
}

int main(int argc, char ** argv) {
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
    int ms = argc > 3 ? atoi(argv[3]) : 1500;
    if (argc >= 2 && !strcmp(argv[1], "myip"))                 return cmd_myip();
    if (argc >= 3 && !strcmp(argv[1], "probe"))                return cmd_probe(argv[2], ms);
    if (argc >= 3 && !strcmp(argv[1], "health"))               return cmd_health(argv[2], ms);
    if (argc >= 3 && !strcmp(argv[1], "plan"))                 return cmd_plan(argv[2]);
    fprintf(stderr, "usage: qwen-helper myip | probe HOST:PORT [ms] | health HOST:PORT [ms] | plan DIR\n");
    return 2;
}

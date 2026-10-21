// qwen-helper: small Win7-compatible helper for the batch scripts (winsock only)
//   qwen-helper myip                         print this PC's LAN IPv4
//   qwen-helper probe HOST:PORT [ms]         exit 0 if a TCP connect succeeds
//   qwen-helper plan WORKERS_DIR             probe registered workers, print "set" lines for main
//   qwen-helper health HOST:PORT [ms]        exit 0 if GET /health answers with "ok"
//   qwen-helper prompt SRC OUT               clean copy of the system prompt (no BOM / CR) for llama-cli -sysf
//   qwen-helper uiconfig SRC OUT [TOOLS]     web UI defaults JSON: system message + JavaScript sandbox tool
//   qwen-helper dashboard ROOT HOST:PORT [auto MIN_WORKERS]
//                                            live cluster status; with "auto" (main PC only) it pauses
//                                            workers that run out of memory and restarts llama-server
// build: x86_64-w64-mingw32-gcc -O2 -D_WIN32_WINNT=0x0601 -DPSAPI_VERSION=2 qwen-helper.c -o qwen-helper.exe -lws2_32 -static
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <time.h>
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


static void read_small(const char * path, char * buf, int cap);

static int is_paused(const char * root_cluster, const char * worker_file) {
    // worker_file is "NAME.txt"; paused marker is cluster\paused\NAME.txt
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s\\..\\paused\\%s", root_cluster, worker_file);
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static int cmd_plan(const char * dir) {
    static char found[64][128]; static char found_name[64][MAX_PATH]; int nf = 0;
    char pattern[MAX_PATH]; snprintf(pattern, sizeof(pattern), "%s\\*.txt", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char path[MAX_PATH]; snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
            if (is_paused(dir, fd.cFileName)) {
                fprintf(stderr, "  worker %-24s PAUSED (delete cluster\\paused\\%s or rerun start_worker.bat to resume)\n",
                        fd.cFileName, fd.cFileName);
                continue;
            }
            FILE * f = fopen(path, "r"); if (!f) continue;
            char line[128] = {0};
            if (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\r\n \t")] = 0;
                if (line[0] && nf < 64) { strcpy(found_name[nf], fd.cFileName); strcpy(found[nf++], line); }
            }
            fclose(f);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    // sort by address, keeping the registration file name with each entry
    for (int a = 0; a < nf; a++) for (int b = a + 1; b < nf; b++) if (strcmp(found[b], found[a]) < 0) {
        char t[MAX_PATH];
        strcpy(t, found[a]); strcpy(found[a], found[b]); strcpy(found[b], t);
        strcpy(t, found_name[a]); strcpy(found_name[a], found_name[b]); strcpy(found_name[b], t);
    }
    char alive[64][128]; int na = 0;
    for (int i = 0; i < nf; i++) {
        int dup = 0;
        for (int j = 0; j < na; j++) if (!strcmp(alive[j], found[i])) dup = 1;
        if (dup) continue;
        int ok = cmd_probe(found[i], 1500) == 0;
        if (!ok) {
            // is that worker alive? (its rpc-server keeps rewriting cluster\status\NAME.txt)
            char sp[MAX_PATH], b1[512], b2[512];
            snprintf(sp, sizeof(sp), "%s\\..\\status\\%s", dir, found_name[i]);
            read_small(sp, b1, sizeof(b1));
            if (b1[0]) {
                Sleep(3500);
                read_small(sp, b2, sizeof(b2));
                if (strcmp(b1, b2) != 0) {
                    // running: retry a few times (it may have just started), then explain
                    for (int k = 0; k < 3 && !ok; k++) { Sleep(2000); ok = cmd_probe(found[i], 2000) == 0; }
                    if (!ok) {
                        fprintf(stderr, "  worker %-24s RUNNING but port 50052 is blocked (skipped)\n", found[i]);
                        fprintf(stderr, "         -> on that PC: run setup_firewall.bat as administrator and delete any\n");
                        fprintf(stderr, "            inbound BLOCK rule for ggml-rpc-server.exe in the firewall settings\n");
                        continue;
                    }
                }
            }
        }
        if (!ok) {
            fprintf(stderr, "  worker %-24s not running (skipped)\n", found[i]);
            continue;
        }
        fprintf(stderr, "  worker %-24s OK\n", found[i]);
        strcpy(alive[na++], found[i]);
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


// ---------------------------------------------------------------- dashboard

static int http_get(const char * hp, const char * path, char * out, int cap, int ms) {
    SOCKET s = connect_to(hp, ms);
    if (s == INVALID_SOCKET) return 0;
    DWORD to = ms; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof(to));
    char req[256]; snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: x\r\n\r\n", path);
    send(s, req, (int)strlen(req), 0);
    int n = 0, r;
    while (n < cap - 1 && (r = recv(s, out + n, cap - 1 - n, 0)) > 0) n += r;
    out[n] = 0; closesocket(s);
    return strstr(out, " 200 ") != NULL;
}

static double metric(const char * text, const char * name) {
    // prometheus text: "llamacpp:NAME VALUE"
    char key[128]; snprintf(key, sizeof(key), "\nllamacpp:%s ", name);
    const char * p = strstr(text, key);
    return p ? atof(p + strlen(key)) : -1;
}

typedef struct {
    char   name[64], ip[64];
    int    registered, paused, has_status, stale, in_use;
    char   phase[16], paused_reason[160];
    double seq, recv_mb, total_mb, avail_mb, proc_mb, peak_mb, page_in, comp_n, comp_avg, comp_max;
    double last_seq; time_t seq_changed;
    int    bad_count;
} worker_t;

static worker_t W[64]; static int NW = 0;

static worker_t * worker_get(const char * name) {
    for (int i = 0; i < NW; i++) if (!_stricmp(W[i].name, name)) return &W[i];
    if (NW >= 64) return NULL;
    memset(&W[NW], 0, sizeof(W[NW])); snprintf(W[NW].name, sizeof(W[NW].name), "%s", name);
    W[NW].last_seq = -1;
    return &W[NW++];
}

static double kv(const char * text, const char * key) {
    char k[64]; snprintf(k, sizeof(k), "%s=", key);
    const char * p = strstr(text, k);
    return p ? atof(p + strlen(k)) : -1;
}

static void read_small(const char * path, char * buf, int cap) {
    buf[0] = 0; FILE * f = fopen(path, "r"); if (!f) return;
    int n = (int)fread(buf, 1, cap - 1, f); buf[n > 0 ? n : 0] = 0; fclose(f);
}

static void scan_dir(const char * dir, int kind) {
    char pattern[MAX_PATH]; snprintf(pattern, sizeof(pattern), "%s\\*.txt", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char name[64]; snprintf(name, sizeof(name), "%s", fd.cFileName);
        char * dot = strrchr(name, '.'); if (dot) *dot = 0;
        worker_t * w = worker_get(name); if (!w) continue;
        char path[MAX_PATH], buf[1024]; snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
        read_small(path, buf, sizeof(buf));
        if (kind == 0) {
            w->registered = 1; buf[strcspn(buf, ":\r\n")] = 0; snprintf(w->ip, sizeof(w->ip), "%s", buf);
        } else if (kind == 1) {
            w->has_status = 1;
            const char * ph = strstr(buf, "phase=");
            if (ph) { snprintf(w->phase, sizeof(w->phase), "%s", ph + 6); w->phase[strcspn(w->phase, "\r\n")] = 0; }
            w->seq = kv(buf, "seq"); w->recv_mb = kv(buf, "recv_mb"); w->total_mb = kv(buf, "mem_total_mb");
            w->avail_mb = kv(buf, "mem_avail_mb"); w->proc_mb = kv(buf, "proc_mb"); w->peak_mb = kv(buf, "proc_peak_mb");
            w->page_in = kv(buf, "page_in_s"); w->comp_n = kv(buf, "compute_n");
            w->comp_avg = kv(buf, "compute_ms_avg"); w->comp_max = kv(buf, "compute_ms_max");
        } else {
            w->paused = 1; buf[strcspn(buf, "\r\n")] = 0; snprintf(w->paused_reason, sizeof(w->paused_reason), "%s", buf);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static DWORD find_process(const char * exe, double * ws_mb) {
    DWORD pid = 0; *ws_mb = -1;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe)) {
        if (!_stricmp(pe.szExeFile, exe)) { pid = pe.th32ProcessID; break; }
    }
    CloseHandle(snap);
    if (pid) {
        HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (ph) {
            PROCESS_MEMORY_COUNTERS pmc; pmc.cb = sizeof(pmc);
            if (GetProcessMemoryInfo(ph, &pmc, sizeof(pmc))) *ws_mb = pmc.WorkingSetSize / 1048576.0;
            CloseHandle(ph);
        }
    }
    return pid;
}

static char EVENTS[8][200]; static int NEV = 0;
static void add_event(const char * fmt, const char * a, const char * b) {
    char msg[160]; snprintf(msg, sizeof(msg), fmt, a, b);
    time_t t = time(NULL); struct tm * tm = localtime(&t);
    if (NEV == 8) { memmove(EVENTS[0], EVENTS[1], sizeof(EVENTS[0]) * 7); NEV = 7; }
    snprintf(EVENTS[NEV++], sizeof(EVENTS[0]), "%02d:%02d:%02d %s", tm->tm_hour, tm->tm_min, tm->tm_sec, msg);
}

static int cmd_dashboard(const char * root, const char * hp, int auto_pause, int min_workers) {
    char d_workers[MAX_PATH], d_status[MAX_PATH], d_paused[MAX_PATH], d_server[MAX_PATH];
    snprintf(d_workers, sizeof(d_workers), "%s\\cluster\\workers", root);
    snprintf(d_status,  sizeof(d_status),  "%s\\cluster\\status",  root);
    snprintf(d_paused,  sizeof(d_paused),  "%s\\cluster\\paused",  root);
    snprintf(d_server,  sizeof(d_server),  "%s\\cluster\\server.txt", root);
    CreateDirectoryA(d_paused, NULL);
    double speed = -1;
    long long slot_task[16], slot_dec[16]; for (int i = 0; i < 16; i++) { slot_task[i] = -1; slot_dec[i] = 0; }
    DWORD t_slots = 0;
    time_t last_pause = 0;
    static char buf[65536];
    for (;;) {
        for (int i = 0; i < NW; i++) { W[i].registered = W[i].has_status = W[i].paused = 0; }
        scan_dir(d_workers, 0); scan_dir(d_status, 1); scan_dir(d_paused, 2);
        time_t now = time(NULL);

        // server side
        int up = http_get(hp, "/health", buf, sizeof(buf), 1500) && strstr(buf, "\"ok\"");
        double processing = -1, deferred = -1;
        if (up && http_get(hp, "/metrics", buf, sizeof(buf), 1500)) {
            deferred = metric(buf, "requests_deferred");
        }
        // live speed from /slots: tokens decoded since the last refresh, summed over busy slots
        if (up && http_get(hp, "/slots", buf, sizeof(buf), 1500)) {
            DWORD t_now = GetTickCount();
            long long new_tokens = 0; int slot = 0; processing = 0;
            for (char * p = strstr(buf, "\"is_processing\":"); p && slot < 16; slot++) {
                char * next = strstr(p + 1, "\"is_processing\":");
                if (next) *next = 0;
                int busy = !strncmp(p + 16, "true", 4);
                char * pt = strstr(p, "\"id_task\":"), * pd = strstr(p, "\"n_decoded\":");
                long long task = pt ? atoll(pt + 10) : -1, dec = pd ? atoll(pd + 12) : 0;
                if (busy) {
                    processing++;
                    new_tokens += (task == slot_task[slot] && dec >= slot_dec[slot]) ? dec - slot_dec[slot] : dec;
                }
                slot_task[slot] = task; slot_dec[slot] = dec;
                if (next) *next = '"';
                p = next;
            }
            double dt = t_slots ? (t_now - t_slots) / 1000.0 : 0;
            speed = (processing > 0 && dt > 0 && new_tokens > 0) ? new_tokens / dt : (processing > 0 ? speed : -1);
            t_slots = t_now;
        }

        // in use = registered, not paused, status fresh and serving
        int n_use = 0, n_paused = 0, n_off = 0;
        double sum_comp = 0, comps[64]; int nc = 0;
        for (int i = 0; i < NW; i++) {
            worker_t * w = &W[i];
            if (w->has_status) {
                if (w->seq != w->last_seq) { w->last_seq = w->seq; w->seq_changed = now; }
                w->stale = now - w->seq_changed > 15;
            } else {
                w->stale = 1;
            }
            w->in_use = w->registered && !w->paused && !w->stale && !strcmp(w->phase, "serving");
            if (w->paused) n_paused++;
            else if (w->in_use) { n_use++; if (w->comp_n > 0) { sum_comp += w->comp_avg; comps[nc++] = w->comp_avg; } }
            else if (w->registered) n_off++;
        }
        double median = 0;
        if (nc) { // tiny sort for the median
            for (int a = 0; a < nc; a++) for (int b = a + 1; b < nc; b++) if (comps[b] < comps[a]) { double t = comps[a]; comps[a] = comps[b]; comps[b] = t; }
            median = comps[nc / 2];
        }

        // draw
        system("cls");
        struct tm * tm = localtime(&now);
        printf(" Qwen3.5-9B cluster status   %02d:%02d:%02d   (refresh 3 s, close this window to exit)\n",
               tm->tm_hour, tm->tm_min, tm->tm_sec);
        printf(" ================================================================================\n");
        if (up) {
            printf(" Server %s : READY   users generating %.0f   waiting %.0f   speed ", hp,
                   processing > 0 ? processing : 0, deferred > 0 ? deferred : 0);
            if (speed > 0 && processing > 1) printf("%.2f t/s total (%.2f t/s per user)\n", speed, speed / processing);
            else if (speed > 0) printf("%.2f t/s\n", speed);
            else printf("-\n");
        } else {
            printf(" Server %s : NOT READY (loading, restarting, or not started)\n", hp);
        }
        double ws; DWORD pid = find_process("llama-server.exe", &ws);
        if (pid) {
            MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms); GlobalMemoryStatusEx(&ms);
            printf(" This PC (main): free %4.0f MB of %4.0f MB   llama-server.exe %4.0f MB\n",
                   ms.ullAvailPhys / 1048576.0, ms.ullTotalPhys / 1048576.0, ws);
        }
        printf(" Workers: %d in use, %d paused, %d other    sum of compute per step: %.0f ms\n",
               n_use, n_paused, n_off, sum_comp);
        printf(" --------------------------------------------------------------------------------\n");
        printf(" %-12s %-15s %-9s %6s %6s %7s %8s  %s\n", "NAME", "IP", "STATE", "FREE", "PROC", "DISK-IN", "MS/STEP", "WARNING");
        printf(" %-12s %-15s %-9s %6s %6s %7s %8s\n", "", "", "", "MB", "MB", "pages/s", "");
        for (int i = 0; i < NW; i++) {
            worker_t * w = &W[i];
            if (!w->registered && !w->paused) continue;
            const char * state = w->paused ? "PAUSED" : w->stale ? "OFFLINE" : w->phase;
            char warn[96] = "";
            int low = w->has_status && !w->stale && w->avail_mb >= 0 && w->avail_mb < 200;
            int paging = w->has_status && !w->stale && w->page_in > 100;
            int slow = w->in_use && nc >= 2 && w->comp_n > 0 && w->comp_avg > 2.5 * median && w->comp_avg > 100;
            if (low) strcat(warn, "LOW-MEMORY ");
            if (paging) strcat(warn, "SWAPPING ");
            if (slow) strcat(warn, "SLOW ");
            if (w->paused) snprintf(warn, sizeof(warn), "%.90s", w->paused_reason);
            if (w->stale && !w->paused) strcpy(warn, "no status (window closed / old version?)");
            char ms_tok[16] = "-";
            if (w->in_use && w->comp_n > 0) snprintf(ms_tok, sizeof(ms_tok), "%.0f", w->comp_avg);
            else if (!strcmp(state, "receiving")) snprintf(ms_tok, sizeof(ms_tok), "%.0fMB", w->recv_mb);
            char c_free[16] = "?", c_proc[16] = "?", c_page[16] = "?";
            if (w->has_status && w->avail_mb >= 0) snprintf(c_free, sizeof(c_free), "%.0f", w->avail_mb);
            if (w->has_status && w->proc_mb >= 0)  snprintf(c_proc, sizeof(c_proc), "%.0f", w->proc_mb);
            if (w->has_status && w->page_in >= 0)  snprintf(c_page, sizeof(c_page), "%.0f", w->page_in);
            printf(" %-12.12s %-15.15s %-9.9s %6s %6s %7s %8s  %s\n", w->name, w->ip, state,
                   c_free, c_proc, c_page, ms_tok, warn);

            // automatic pause (main PC only): sustained low memory or swapping while serving
            w->bad_count = (w->in_use && (low || paging)) ? w->bad_count + 1 : 0;
            if (auto_pause && pid && w->bad_count >= 6 && now - last_pause > 120) {
                if (n_use - 1 >= min_workers) {
                    char path[MAX_PATH], reason[160];
                    snprintf(path, sizeof(path), "%s\\%s.txt", d_paused, w->name);
                    snprintf(reason, sizeof(reason), "paused %02d:%02d: free %.0f MB, disk-in %.0f pages/s",
                             tm->tm_hour, tm->tm_min, w->avail_mb, w->page_in);
                    FILE * f = fopen(path, "w"); if (f) { fprintf(f, "%s\n", reason); fclose(f); }
                    HANDLE ph = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (ph) { TerminateProcess(ph, 1); CloseHandle(ph); }
                    add_event("%s paused (%s); server restarting without it", w->name, reason);
                    last_pause = now; w->bad_count = 0;
                } else if (w->bad_count == 6) {
                    add_event("%s is short of memory, but pausing it would leave fewer than MIN_WORKERS%s", w->name, "");
                }
            }
        }
        printf(" --------------------------------------------------------------------------------\n");
        printf(" FREE = free memory on that PC. DISK-IN > 100 = Windows is swapping (very slow).\n");
        printf(" MS/STEP = compute time per step on that worker (one step = 1 token for every user);\n");
        printf("           one worker much higher than the others = the bottleneck.\n");
        if (speed > 0 && sum_comp > 0 && processing == 1) {
            double total = 1000.0 / speed;
            printf(" Per token: %.0f ms total = %.0f ms compute on workers + %.0f ms network/main/other\n",
                   total, sum_comp, total > sum_comp ? total - sum_comp : 0);
        }
        if (auto_pause) printf(" Auto-pause is ON (keeps at least %d workers). Resume: rerun start_worker.bat there.\n", min_workers);
        if (NEV) { printf(" Events:\n"); for (int i = 0; i < NEV; i++) printf("  %s\n", EVENTS[i]); }
        (void) d_server;
        Sleep(3000);
    }
    return 0;
}

// ---------------------------------------------------------------- system prompt

// reads a UTF-8 text file (Notepad may add a BOM) into a malloc'd, BOM-less, CR-less string
static char * read_prompt(const char * path) {
    FILE * f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 65536) { fclose(f); return NULL; }
    char * buf = (char *) malloc(n + 1);
    size_t got = fread(buf, 1, n, f); fclose(f);
    buf[got] = 0;
    char * s = buf;
    if ((unsigned char) s[0] == 0xEF && (unsigned char) s[1] == 0xBB && (unsigned char) s[2] == 0xBF) s += 3;
    char * w = buf;
    for (char * r = s; *r; r++) if (*r != '\r') *w++ = *r;
    *w = 0;
    while (w > buf && (w[-1] == '\n' || w[-1] == ' ')) *--w = 0;
    return buf;
}

// prompt SRC OUT: clean copy for llama-cli -sysf
static int cmd_prompt(const char * src, const char * out) {
    char * p = read_prompt(src);
    if (!p) return 1;
    FILE * f = fopen(out, "wb");
    if (!f) { free(p); return 1; }
    fputs(p, f); fclose(f); free(p);
    return 0;
}

// uiconfig SRC OUT TOOLS: default web UI settings (system message, JavaScript sandbox tool)
static int cmd_uiconfig(const char * src, const char * out, int tools) {
    char * p = read_prompt(src);
    FILE * f = fopen(out, "wb");
    if (!f) { free(p); return 1; }
    fputs("{\"systemMessage\":\"", f);
    for (const char * c = p ? p : ""; *c; c++) {
        unsigned char ch = (unsigned char) *c;
        if (ch == '"' || ch == '\\') { fputc('\\', f); fputc(ch, f); }
        else if (ch == '\n') fputs("\\n", f);
        else if (ch == '\t') fputs("\\t", f);
        else if (ch < 0x20) continue;
        else fputc(ch, f);
    }
    fprintf(f, "\",\"jsSandboxEnabled\":%s}", tools ? "true" : "false");
    fclose(f); free(p);
    return 0;
}

int main(int argc, char ** argv) {
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
    int ms = argc > 3 ? atoi(argv[3]) : 1500;
    if (argc >= 2 && !strcmp(argv[1], "myip"))                 return cmd_myip();
    if (argc >= 3 && !strcmp(argv[1], "probe"))                return cmd_probe(argv[2], ms);
    if (argc >= 3 && !strcmp(argv[1], "health"))               return cmd_health(argv[2], ms);
    if (argc >= 3 && !strcmp(argv[1], "plan"))                 return cmd_plan(argv[2]);
    if (argc >= 4 && !strcmp(argv[1], "prompt"))               return cmd_prompt(argv[2], argv[3]);
    if (argc >= 4 && !strcmp(argv[1], "uiconfig"))             return cmd_uiconfig(argv[2], argv[3], argc >= 5 ? atoi(argv[4]) : 1);
    if (argc >= 4 && !strcmp(argv[1], "dashboard"))
        return cmd_dashboard(argv[2], argv[3], argc >= 5 && !strcmp(argv[4], "auto"), argc >= 6 ? atoi(argv[5]) : 3);
    fprintf(stderr, "usage: qwen-helper myip | probe HOST:PORT [ms] | health HOST:PORT [ms] | plan DIR | prompt SRC OUT | uiconfig SRC OUT [0|1] | dashboard ROOT HOST:PORT [auto MIN]\n");
    return 2;
}

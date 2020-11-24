#ifndef DLUA_COMMON_H
#define DLUA_COMMON_H

#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <typeinfo>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <unordered_map>
#include <fcntl.h>
#include <sstream>
#include <algorithm>
#include <vector>
#include <unordered_set>
#include <set>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>

const int open_debug = 1;

#define DLOG(...) if (open_debug) {dlog(stdout, "[DEBUG] ", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);}
#define DERR(...) if (open_debug) {dlog(stderr, "[ERROR] ", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);}

void dlog(FILE *fd, const char *header, const char *file, const char *func, int pos, const char *fmt, ...) {
    time_t clock1;
    struct tm *tptr;
    va_list ap;

    clock1 = time(0);
    tptr = localtime(&clock1);

    struct timeval tv;
    gettimeofday(&tv, NULL);

    fprintf(fd, "%s[%d.%d.%d,%d:%d:%d,%llu]%s:%d,%s: ", header,
            tptr->tm_year + 1900, tptr->tm_mon + 1,
            tptr->tm_mday, tptr->tm_hour, tptr->tm_min,
            tptr->tm_sec, (long long) ((tv.tv_usec) / 1000) % 1000, file, pos, func);

    va_start(ap, fmt);
    vfprintf(fd, fmt, ap);
    fprintf(fd, "\n");
    va_end(ap);
}

const int QUEUED_MESSAGE_MSG_LEN = 127;
struct QueuedMessage {
    long type;
    char payload[QUEUED_MESSAGE_MSG_LEN + 1];
};

const int HB_MSG = 0;

static int send_msg(int qid, int type, const char *data) {
    QueuedMessage msg;
    msg.type = type;
    strncpy(msg.payload, data, QUEUED_MESSAGE_MSG_LEN);
    msg.payload[QUEUED_MESSAGE_MSG_LEN] = 0;
    int msgsz = strlen(msg.payload);
    if (msgsnd(qid, &msg, msgsz, 0) != 0) {
        DERR("send_msg %d %d error %d %s", qid, msgsz, errno, strerror(errno));
        return -1;
    }
    return 0;
}

static int open_msg_queue(const char *tmpfilename, int pid) {

    int tmpfd = open(tmpfilename, O_RDWR | O_CREAT, 0777);
    if (tmpfd == -1) {
        DERR("open tmpfile fail %s %d %s", tmpfilename, errno, strerror(errno));
        return -1;
    }
    close(tmpfd);

    key_t key = ftok(tmpfilename, pid);
    if (key == -1) {
        DERR("ftok fail %s %d %d %s", tmpfilename, pid, errno, strerror(errno));
        return -1;
    }

    DLOG("ftok key %d", key);

    int qid = msgget(key, 0666 | IPC_CREAT);
    if (qid < 0) {
        DERR("msgget fail %s %d %s", key, errno, strerror(errno));
        return -1;
    }

    msqid_ds buf;
    if (msgctl(qid, IPC_STAT, &buf) == -1) {
        DERR("msgctl get fail %s %d %s", qid, errno, strerror(errno));
        return -1;
    }

    DLOG("before msg_qbytes %d", buf.msg_qbytes);

    buf.msg_qbytes = 1024 * 1024;
    if (msgctl(qid, IPC_SET, &buf) == -1) {
        DERR("msgctl set fail %s %d %s", qid, errno, strerror(errno));
        return -1;
    }

    if (msgctl(qid, IPC_STAT, &buf) == -1) {
        DERR("msgctl get fail %s %d %s", qid, errno, strerror(errno));
        return -1;
    }

    DLOG("after msg_qbytes %d", buf.msg_qbytes);

    DLOG("qid %d", qid);

    return qid;
}

std::string exec_command(const char *cmd, int &out) {
    out = 0;
    auto pPipe = ::popen(cmd, "r");
    if (pPipe == nullptr) {
        out = -1;
        return "";
    }

    std::array<char, 256> buffer;

    std::string result;

    while (not std::feof(pPipe)) {
        auto bytes = std::fread(buffer.data(), 1, buffer.size(), pPipe);
        result.append(buffer.data(), bytes);
    }

    auto rc = ::pclose(pPipe);

    if (WIFEXITED(rc)) {
        out = WEXITSTATUS(rc);
    }

    return result;
}

#endif //DLUA_COMMON_H

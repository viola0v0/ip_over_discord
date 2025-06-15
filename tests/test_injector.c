#include "http_server.h"
#include "injector.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(){
    int fds[2];
    if(pipe(fds)<0){ perror("pipe"); return 1; }
    int read_fd = fds[0], write_fd = fds[1];
    http_server_start("8000");
    injector_start(write_fd, "127.0.0.1", 8000);
    sleep(1);
    const char *msg = "injector_test";
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "curl -s -X POST --data-binary '%s' http://127.0.0.1:8000/send",
        msg);
    system(cmd);
    char buf[128];
    ssize_t n = read(read_fd, buf, sizeof(buf));
    if(n>0) printf("recv %zd: %.*s\n", n, (int)n, buf);
    else { perror("read"); return 1; }
    injector_stop();
    http_server_stop();
    return 0;
}

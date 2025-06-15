#include "tun.h"
#include "injector.h"
#include "http_server.h"
#include <net/if.h>
#include <linux/if_tun.h>
#include <unistd.h>

int main(){
    char dev[IF_NAMESIZE] = "tun1";
    int fd = tun_alloc(dev, IFF_TUN | IFF_NO_PI);
    if(fd < 0) return 1;
    http_server_start("8000");
    injector_start(fd, "127.0.0.1", 8000);
    pause();
    injector_stop();
    http_server_stop();
    return 0;
}

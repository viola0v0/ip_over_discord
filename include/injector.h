//  /include/injector.h
#ifndef INJECTOR_H
#define INJECTOR_H

#include <stddef.h>

int injector_start(int tun_fd);

void injector_stop(void);

int injector_add_route(const char *subnet_cidr, const char *tun_name);

#endif // INJECTOR_H

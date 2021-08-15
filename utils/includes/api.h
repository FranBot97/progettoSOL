//
// Created by francesco on 15/08/21.
//

#ifndef API_H
#define API_H
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>


int openConnection(const char* sockname, int msec, const struct timespec abstime);


#endif //API_H

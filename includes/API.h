//
// Created by linuxlite on 02/03/22.
//

#ifndef NOVEMBREPROG_API_H
#define NOVEMBREPROG_API_H
#include <stdbool.h>
#include <list.h>
#include <util.h>
#include <constant_values.h>

int sendToServer(void* content, size_t len, const char* contentType);

void* recieveFromServer(const char* contentType);

int unlockFile(const char* pathname);

int openConnection(const char* sockname, int msec, const struct timespec abstime);

int closeConnection(const char* sockname);

int openFile(const char* pathname, int flags);

int readFile(const char* pathname, void** buf, size_t* size);int readNFiles(int N, const char* dirname);

int writeFile(const char* pathname, const char* dirname);

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

int lockFile(const char* pathname);

void reset();

#endif //NOVEMBREPROG_API_H

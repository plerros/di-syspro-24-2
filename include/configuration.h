#ifndef SYSPROG24_2_CONFIGURATION_H
#define SYSPROG24_2_CONFIGURATION_H

#include <stdbool.h>

#define SERVER_NAME "./jobExecutorServer"
#define TXT_NAME "jobExecutorServer.txt"

#define HANDSHAKE "handshake"

#define PACKET_SIZE 64
#define LINK_SIZE 100

#define SAFETY_CHECKS true

#define QUEUE_CLEAR true
/*
 * Stray away from the requested implementation for more speed!
 */

#define STOP_RUNNING_JOBS false

#endif /*SYSPROG24_2_CONFIGURATION_H*/
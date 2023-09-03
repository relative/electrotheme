#ifndef SERVICE_NODE_HPP
#define SERVICE_NODE_HPP

#include <stdint.h>

void node_debug_process(uint32_t pid, void* handle);
bool node_debuggable_process(uint32_t pid);

#endif /* SERVICE_NODE_HPP */

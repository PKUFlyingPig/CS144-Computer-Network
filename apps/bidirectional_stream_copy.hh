#ifndef SPONGE_APPS_BIDIRECTIONAL_STREAM_COPY_HH
#define SPONGE_APPS_BIDIRECTIONAL_STREAM_COPY_HH

#include "socket.hh"

//! Copy socket input/output to stdin/stdout until finished
void bidirectional_stream_copy(Socket &socket);

#endif  // SPONGE_APPS_BIDIRECTIONAL_STREAM_COPY_HH

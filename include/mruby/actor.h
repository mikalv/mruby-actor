#ifndef MRUBY_ACTOR_H
#define MRUBY_ACTOR_H

#include <mruby.h>
#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

MRB_API void
mrb_zactor_fn(zsock_t* pipe, void* name);

#ifdef __cplusplus
}
#endif

#endif

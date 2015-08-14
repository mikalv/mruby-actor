#ifndef MRUBY_ACTOR_H
#define MRUBY_ACTOR_H

#include <mruby.h>

#ifdef __cplusplus
extern "C" {
#endif

#define E_ACTOR_PROTOCOL_ERROR mrb_class_get_under(mrb, mrb_class_get(mrb, "Actor"), "ProtocolError")

#ifdef __cplusplus
}
#endif

#endif

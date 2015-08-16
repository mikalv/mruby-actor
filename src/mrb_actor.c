#include <stdio.h>
#include <mruby.h>
#include <czmq.h>
#include "mrb_actor_msg.h"
#include <assert.h>
#include <string.h>
#include <mruby/throw.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/variable.h>
#include <mruby/compile.h>
#include <mruby/error.h>
#include <mruby/data.h>
#include <errno.h>
#include <mruby/string.h>
#include <mruby/value.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include "mruby/actor.h"

typedef struct {
  char* mrb_file;
  FILE* mrb_file_handle;
  mrb_state* mrb;
  zloop_t* reactor;
  mrb_actor_msg_t* actor_msg;
  zsock_t* router;
  zsock_t* pull;
  zsock_t* pub;
} self_t;

static void
s_self_destroy(self_t** self_p)
{
  assert(self_p);

  if (*self_p) {
    self_t* self = *self_p;
    zstr_free(&self->mrb_file);
    if (self->mrb_file_handle) {
      fclose(self->mrb_file_handle);
      self->mrb_file_handle = NULL;
    }
    if (self->mrb) {
      mrb_close(self->mrb);
      self->mrb = NULL;
    }
    zloop_destroy(&self->reactor);
    mrb_actor_msg_destroy(&self->actor_msg);
    zsock_destroy(&self->router);
    zsock_destroy(&self->pull);
    zsock_destroy(&self->pub);
    free(self);
    *self_p = NULL;
  }
}

static int
mrb_actor_pipe_reader(zloop_t* reactor, zsock_t* pipe, void* args)
{
  self_t* self = (self_t*)args;
  int rc = 0;
  zmsg_t* msg = zmsg_recv(pipe);
  if (!msg)
    return -1;

  char* command = zmsg_popstr(msg);
  zsys_debug("command: %s", command);
  if (streq(command, "$TERM"))
    rc = -1;
  else if (streq(command, "BIND ROUTER")) {
    char* endpoint = zmsg_popstr(msg);
    if (zsock_bind(self->router, "%s", endpoint) == -1) {
      zsys_warning("could not bind router to %s (%s)", endpoint, zmq_strerror(zmq_errno()));
      zsock_signal(pipe, 1);
    }
    else
      zsock_signal(pipe, 0);
    zstr_free(&endpoint);
  }
  else if (streq(command, "BIND PULL")) {
    char* endpoint = zmsg_popstr(msg);
    if (zsock_bind(self->pull, "%s", endpoint) == -1) {
      zsys_warning("could not bind pull to %s (%s)", endpoint, zmq_strerror(zmq_errno()));
      zsock_signal(pipe, 1);
    }
    else
      zsock_signal(pipe, 0);
    zstr_free(&endpoint);
  }
  else if (streq(command, "BIND PUB")) {
    char* endpoint = zmsg_popstr(msg);
    if (zsock_bind(self->pub, "%s", endpoint) == -1) {
      zsys_warning("could not bind pub to %s (%s)", endpoint, zmq_strerror(zmq_errno()));
      zsock_signal(pipe, 1);
    }
    else
      zsock_signal(pipe, 0);
    zstr_free(&endpoint);
  }

  zstr_free(&command);
  zmsg_destroy(&msg);

  return rc;
}

static int
mrb_actor_router_reader(zloop_t* reactor, zsock_t* router, void* args)
{
  self_t* self = (self_t*)args;
  mrb_state* mrb = self->mrb;
  int ai = mrb_gc_arena_save(mrb);

  int rc = mrb_actor_msg_recv(self->actor_msg, router);
  if (rc == -1)
    return rc;

  switch (mrb_actor_msg_id(self->actor_msg)) {
  case MRB_ACTOR_MSG_INITIALIZE: {
    const char* mrb_actor_class_cstr = mrb_actor_msg_mrb_class(self->actor_msg);
    zchunk_t* args = mrb_actor_msg_args(self->actor_msg);
    struct mrb_jmpbuf* prev_jmp = mrb->jmp;
    struct mrb_jmpbuf c_jmp;

    MRB_TRY(&c_jmp)
    {
      mrb->jmp = &c_jmp;
      mrb_value mrb_actor_class_str = mrb_str_new_static(mrb, mrb_actor_class_cstr, strlen(mrb_actor_class_cstr));
      mrb_value mrb_actor_class_obj = mrb_funcall(mrb, mrb_actor_class_str, "constantize", 0);
      struct RClass* mrb_actor_class_ptr = mrb_class_ptr(mrb_actor_class_obj);
      mrb_value obj;
      if (zchunk_size(args) > 0) {
        mrb_value args_str = mrb_str_new_static(mrb, zchunk_data(args), zchunk_size(args));
        mrb_value args_obj = mrb_funcall(mrb, mrb_obj_value(mrb_module_get(mrb, "MessagePack")), "unpack", 1, args_str);
        if (!mrb_array_p(args_obj))
          mrb_raise(mrb, E_ARGUMENT_ERROR, "args must be a Array");
        obj = mrb_obj_new(mrb, mrb_actor_class_ptr, RARRAY_LEN(args_obj), RARRAY_PTR(args_obj));
      }
      else
        obj = mrb_obj_new(mrb, mrb_actor_class_ptr, 0, NULL);

      mrb_sym actor_state_sym = mrb_intern_lit(mrb, "mruby_actor_state");
      mrb_value actor_state = mrb_gv_get(mrb, actor_state_sym);
      mrb_int object_id = mrb_obj_id(obj);
      mrb_value object_id_val = mrb_fixnum_value(object_id);
      mrb_hash_set(mrb, actor_state, object_id_val, obj);
      mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_INITIALIZE_OK);
      mrb_actor_msg_set_object_id(self->actor_msg, object_id);
      mrb->jmp = prev_jmp;
    }
    MRB_CATCH(&c_jmp)
    {
      mrb->jmp = prev_jmp;
      mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_ERROR);
      mrb_value exc = mrb_obj_value(mrb->exc);
      mrb_actor_msg_set_mrb_class(self->actor_msg, mrb_obj_classname(mrb, exc));
      mrb_value exc_str = mrb_funcall(mrb, exc, "to_s", 0);
      const char* exc_msg = mrb_string_value_cstr(mrb, &exc_str);
      mrb_actor_msg_set_error(self->actor_msg, exc_msg);
      mrb->exc = NULL;
    }
    MRB_END_EXC(&c_jmp);
  } break;
  case MRB_ACTOR_MSG_SEND_MESSAGE: {
    uint64_t object_id = mrb_actor_msg_object_id(self->actor_msg);
    const char* method = mrb_actor_msg_method(self->actor_msg);
    zchunk_t* args = mrb_actor_msg_args(self->actor_msg);
    struct mrb_jmpbuf* prev_jmp = mrb->jmp;
    struct mrb_jmpbuf c_jmp;

    MRB_TRY(&c_jmp)
    {
      mrb->jmp = &c_jmp;
      mrb_sym actor_state_sym = mrb_intern_lit(mrb, "mruby_actor_state");
      mrb_value actor_state = mrb_gv_get(mrb, actor_state_sym);
      mrb_value object_id_val = mrb_fixnum_value(object_id);
      mrb_value obj = mrb_hash_get(mrb, actor_state, object_id_val);
      mrb_value result;
      if (zchunk_size(args) > 0) {
        mrb_value args_str = mrb_str_new_static(mrb, zchunk_data(args), zchunk_size(args));
        mrb_value args_obj = mrb_funcall(mrb, mrb_obj_value(mrb_module_get(mrb, "MessagePack")), "unpack", 1, args_str);
        if (!mrb_array_p(args_obj))
          mrb_raise(mrb, E_ARGUMENT_ERROR, "args must be a Array");
        mrb_sym method_sym = mrb_intern_cstr(mrb, method);
        result = mrb_funcall_argv(mrb, obj, method_sym, RARRAY_LEN(args_obj), RARRAY_PTR(args_obj));
      }
      else
        result = mrb_funcall(mrb, obj, method, 0);

      mrb_value result_str = mrb_funcall(mrb, result, "to_msgpack", 0);
      zchunk_t* result_chunk = zchunk_new(RSTRING_PTR(result_str), RSTRING_LEN(result_str));
      mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_SEND_OK);
      mrb_actor_msg_set_result(self->actor_msg, &result_chunk);
    }
    MRB_CATCH(&c_jmp)
    {
      mrb->jmp = prev_jmp;
      mrb_value exc = mrb_obj_value(mrb->exc);
      mrb_value exc_str = mrb_funcall(mrb, exc, "to_s", 0);
      const char* exc_msg = mrb_string_value_cstr(mrb, &exc_str);
      mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_ERROR);
      mrb_actor_msg_set_mrb_class(self->actor_msg, mrb_obj_classname(mrb, mrb_obj_value(mrb->exc)));
      mrb_actor_msg_set_error(self->actor_msg, exc_msg);
      mrb->exc = NULL;
    }
    MRB_END_EXC(&c_jmp);
  } break;
  default: {
    mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_ERROR);
    mrb_actor_msg_set_mrb_class(self->actor_msg, "Actor::ProtocolError");
    mrb_actor_msg_set_error(self->actor_msg, "Invalid Message recieved");
  }
  }

  rc = mrb_actor_msg_send(self->actor_msg, router);
  mrb_gc_arena_restore(mrb, ai);

  return rc;
}

static int
mrb_actor_pull_reader(zloop_t* reactor, zsock_t* pull, void* args)
{
  self_t* self = (self_t*)args;
  mrb_state* mrb = self->mrb;
  int ai = mrb_gc_arena_save(mrb);

  int rc = mrb_actor_msg_recv(self->actor_msg, pull);
  if (rc == -1)
    return rc;

  switch (mrb_actor_msg_id(self->actor_msg)) {
  case MRB_ACTOR_MSG_ASYNC_SEND_MESSAGE: {
    uint64_t object_id = mrb_actor_msg_object_id(self->actor_msg);
    const char* method = mrb_actor_msg_method(self->actor_msg);
    zchunk_t* args = mrb_actor_msg_args(self->actor_msg);
    struct mrb_jmpbuf* prev_jmp = mrb->jmp;
    struct mrb_jmpbuf c_jmp;

    MRB_TRY(&c_jmp)
    {
      mrb->jmp = &c_jmp;
      mrb_sym actor_state_sym = mrb_intern_lit(mrb, "mruby_actor_state");
      mrb_value actor_state = mrb_gv_get(mrb, actor_state_sym);
      mrb_value object_id_val = mrb_fixnum_value(object_id);
      mrb_value obj = mrb_hash_get(mrb, actor_state, object_id_val);
      mrb_value result;
      if (zchunk_size(args) > 0) {
        mrb_value args_str = mrb_str_new_static(mrb, zchunk_data(args), zchunk_size(args));
        mrb_value args_obj = mrb_funcall(mrb, mrb_obj_value(mrb_module_get(mrb, "MessagePack")), "unpack", 1, args_str);
        if (!mrb_array_p(args_obj))
          mrb_raise(mrb, E_ARGUMENT_ERROR, "args must be a Array");
        mrb_sym method_sym = mrb_intern_cstr(mrb, method);
        result = mrb_funcall_argv(mrb, obj, method_sym, RARRAY_LEN(args_obj), RARRAY_PTR(args_obj));
      }
      else
        result = mrb_funcall(mrb, obj, method, 0);

      mrb_value result_str = mrb_funcall(mrb, result, "to_msgpack", 0);
      zchunk_t* result_chunk = zchunk_new(RSTRING_PTR(result_str), RSTRING_LEN(result_str));
      mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_ASYNC_SEND_OK);
      mrb_actor_msg_set_result(self->actor_msg, &result_chunk);
    }
    MRB_CATCH(&c_jmp)
    {
      mrb->jmp = prev_jmp;
      mrb_value exc = mrb_obj_value(mrb->exc);
      mrb_value exc_str = mrb_funcall(mrb, exc, "to_s", 0);
      const char* exc_msg = mrb_string_value_cstr(mrb, &exc_str);
      mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_ASYNC_ERROR);
      mrb_actor_msg_set_mrb_class(self->actor_msg, mrb_obj_classname(mrb, mrb_obj_value(mrb->exc)));
      mrb_actor_msg_set_error(self->actor_msg, exc_msg);
      mrb->exc = NULL;
    }
    MRB_END_EXC(&c_jmp);
  } break;
  default: {
    mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_ASYNC_ERROR);
    mrb_actor_msg_set_mrb_class(self->actor_msg, "Actor::ProtocolError");
    mrb_actor_msg_set_error(self->actor_msg, "Invalid Message recieved");
  }
  }

  rc = mrb_actor_msg_send(self->actor_msg, self->pub);
  mrb_gc_arena_restore(mrb, ai);

  return rc;
}

static self_t*
s_self_new(zsock_t* pipe, const char* mrb_file)
{
  assert(pipe);
  assert(mrb_file);
  assert(strlen(mrb_file) > 0);

  int rc = -1;
  self_t* self = (self_t*)zmalloc(sizeof(self_t));
  if (!self)
    return NULL;

  self->mrb_file = strdup(mrb_file);
  if (self->mrb_file)
    self->mrb_file_handle = fopen(self->mrb_file, "r");
  if (self->mrb_file_handle)
    self->mrb = mrb_open();
  if (self->mrb) {
    mrb_state* mrb = self->mrb;
    struct mrb_jmpbuf* prev_jmp = mrb->jmp;
    struct mrb_jmpbuf c_jmp;
    MRB_TRY(&c_jmp)
    {
      mrb->jmp = &c_jmp;
      mrb_value ret = mrb_load_file(mrb, self->mrb_file_handle);
      if (mrb_undef_p(ret))
        mrb_exc_raise(mrb, mrb_obj_value(mrb->exc));
      mrb_sym actor_state_sym = mrb_intern_lit(mrb, "mruby_actor_state");
      mrb_value actor_state = mrb_hash_new(mrb);
      mrb_gv_set(mrb, actor_state_sym, actor_state);
      self->reactor = zloop_new();
      mrb->jmp = prev_jmp;
    }
    MRB_CATCH(&c_jmp)
    {
      mrb->jmp = prev_jmp;
      mrb_p(mrb, mrb_obj_value(mrb->exc));
    }
    MRB_END_EXC(&c_jmp);
  }
  if (self->reactor) {
    zloop_ignore_interrupts(self->reactor);
    rc = zloop_reader(self->reactor, pipe, mrb_actor_pipe_reader, self);
  }
  if (rc == 0)
    self->actor_msg = mrb_actor_msg_new();
  if (self->actor_msg)
    self->router = zsock_new(ZMQ_ROUTER);
  if (self->router)
    rc = zloop_reader(self->reactor, self->router, mrb_actor_router_reader, self);
  if (rc == 0)
    self->pull = zsock_new(ZMQ_PULL);
  if (self->pull)
    rc = zloop_reader(self->reactor, self->pull, mrb_actor_pull_reader, self);
  if (rc == 0)
    self->pub = zsock_new(ZMQ_PUB);
  if (!self->pub)
    s_self_destroy(&self);

  return self;
}

static void
mrb_zactor_fn(zsock_t* pipe, void* mrb_file)
{
  self_t* self = s_self_new(pipe, (const char*)mrb_file);
  assert(self);

  zsock_signal(pipe, 0);

  zloop_start(self->reactor);

  s_self_destroy(&self);
  zsock_signal(pipe, 0);
}

static mrb_value
mrb_actor_initialize(mrb_state* mrb, mrb_value self)
{
  errno = 0;
  mrb_value mrb_file;
  char *router_endpoint = NULL, *pull_endpoint = NULL, *pub_endpoint = NULL;

  mrb_get_args(mrb, "S|zzz", &mrb_file, &router_endpoint, &pull_endpoint, &pub_endpoint);

  mrb_value dealer_args[1];
  dealer_args[0] = mrb_fixnum_value(ZMQ_DEALER);
  mrb_value dealer = mrb_obj_new(mrb, mrb_class_get_under(mrb, mrb_module_get(mrb, "CZMQ"), "Zsock"), 1, dealer_args);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "dealer"), dealer);

  mrb_value push_args[1];
  push_args[0] = mrb_fixnum_value(ZMQ_PUSH);
  mrb_value push = mrb_obj_new(mrb, mrb_class_get_under(mrb, mrb_module_get(mrb, "CZMQ"), "Zsock"), 1, push_args);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "push"), push);

  mrb_value actor_msg_obj = mrb_obj_new(mrb, mrb_class_get_under(mrb, mrb_class(mrb, self), "Msg"), 0, NULL);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "actor_msg"), actor_msg_obj);

  mrb_value actor_args[2];
  actor_args[0] = mrb_cptr_value(mrb, mrb_zactor_fn);
  actor_args[1] = mrb_file;

  mrb_value zactor = mrb_obj_new(mrb,
      mrb_class_get_under(mrb,
                                     mrb_module_get(mrb, "CZMQ"), "Zactor"),
      2, actor_args);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "zactor"), zactor);

  if (router_endpoint) {
    if (zsock_send(DATA_PTR(zactor), "ss", "BIND ROUTER", router_endpoint) == 0 && zsock_wait(DATA_PTR(zactor)) == 0)
      zsock_connect(DATA_PTR(dealer), "%s", router_endpoint);
    else
      mrb_sys_fail(mrb, "zactor_new_mrb_zactor_fn");
  }
  if (pull_endpoint) {
    if (zsock_send(DATA_PTR(zactor), "ss", "BIND PULL", pull_endpoint) == 0 && zsock_wait(DATA_PTR(zactor)) == 0)
      zsock_connect(DATA_PTR(push), "%s", pull_endpoint);
    else
      mrb_sys_fail(mrb, "zactor_new_mrb_zactor_fn");
  }
  if (pub_endpoint) {
    if (zsock_send(DATA_PTR(zactor), "ss", "BIND PUB", pub_endpoint) == 0 && zsock_wait(DATA_PTR(zactor)) == 0) {
    }
    else
      mrb_sys_fail(mrb, "zactor_new_mrb_zactor_fn");
  }

  return self;
}

static mrb_value
mrb_actor_init(mrb_state* mrb, mrb_value self)
{
  errno = 0;
  char* mrb_actor_class;
  mrb_value* argv;
  mrb_int argc = 0;

  mrb_get_args(mrb, "z*", &mrb_actor_class, &argv, &argc);

  mrb_value actor_msg_obj = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "actor_msg"));
  mrb_actor_msg_t* actor_msg = (mrb_actor_msg_t*)DATA_PTR(actor_msg_obj);
  mrb_actor_msg_set_id(actor_msg, MRB_ACTOR_MSG_INITIALIZE);
  mrb_actor_msg_set_mrb_class(actor_msg, mrb_actor_class);

  if (argc > 0) {
    mrb_value args_ary = mrb_ary_new_from_values(mrb, argc, argv);
    mrb_value args_str = mrb_funcall(mrb, args_ary, "to_msgpack", 0);
    zchunk_t* args = zchunk_new(RSTRING_PTR(args_str), RSTRING_LEN(args_str));
    mrb_actor_msg_set_args(actor_msg, &args);
  }

  mrb_sym dealer_sym = mrb_intern_lit(mrb, "dealer");
  mrb_value dealer = mrb_iv_get(mrb, self, dealer_sym);
  if (mrb_actor_msg_send(actor_msg, (zsock_t*)DATA_PTR(dealer)) == 0 && mrb_actor_msg_recv(actor_msg, (zsock_t*)DATA_PTR(dealer)) == 0) {
    switch (mrb_actor_msg_id(actor_msg)) {
    case MRB_ACTOR_MSG_INITIALIZE_OK: {
      mrb_value proxy_args[2];
      proxy_args[0] = self;
      proxy_args[1] = mrb_fixnum_value(mrb_actor_msg_object_id(actor_msg));
      return mrb_obj_new(mrb,
          mrb_class_get_under(mrb,
                             mrb_class(mrb, self), "Proxy"),
          2, proxy_args);
    } break;
    case MRB_ACTOR_MSG_ERROR: {
      const char* mrb_actor_class_cstr = mrb_actor_msg_mrb_class(actor_msg);
      mrb_value mrb_actor_class_str = mrb_str_new_static(mrb, mrb_actor_class_cstr, strlen(mrb_actor_class_cstr));
      mrb_value mrb_actor_class_obj = mrb_funcall(mrb, mrb_actor_class_str, "constantize", 0);
      mrb_raise(mrb, mrb_class_ptr(mrb_actor_class_obj), mrb_actor_msg_error(actor_msg));
    } break;
    default: {
      mrb_raise(mrb, E_ACTOR_PROTOCOL_ERROR, "Invalid Message recieved");
    }
    }
  }
  else
    mrb_sys_fail(mrb, "mrb_actor_init");

  return self;
}

static mrb_value
mrb_actor_send(mrb_state* mrb, mrb_value self)
{
  errno = 0;
  mrb_int object_id;
  mrb_sym method;
  mrb_value* argv;
  mrb_int argc = 0;

  mrb_get_args(mrb, "in*", &object_id, &method, &argv, &argc);

  mrb_value actor_msg_obj = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "actor_msg"));
  mrb_actor_msg_t* actor_msg = (mrb_actor_msg_t*)DATA_PTR(actor_msg_obj);
  mrb_actor_msg_set_id(actor_msg, MRB_ACTOR_MSG_SEND_MESSAGE);
  mrb_actor_msg_set_object_id(actor_msg, object_id);
  mrb_actor_msg_set_method(actor_msg, mrb_sym2name(mrb, method));

  if (argc > 0) {
    mrb_value args_ary = mrb_ary_new_from_values(mrb, argc, argv);
    mrb_value args_str = mrb_funcall(mrb, args_ary, "to_msgpack", 0);
    zchunk_t* args = zchunk_new(RSTRING_PTR(args_str), RSTRING_LEN(args_str));
    mrb_actor_msg_set_args(actor_msg, &args);
  }

  mrb_sym dealer_sym = mrb_intern_lit(mrb, "dealer");
  mrb_value dealer = mrb_iv_get(mrb, self, dealer_sym);
  if (mrb_actor_msg_send(actor_msg, (zsock_t*)DATA_PTR(dealer)) == 0 && mrb_actor_msg_recv(actor_msg, (zsock_t*)DATA_PTR(dealer)) == 0) {
    switch (mrb_actor_msg_id(actor_msg)) {
    case MRB_ACTOR_MSG_SEND_OK: {
      zchunk_t* result = mrb_actor_msg_result(actor_msg);
      mrb_value result_obj = mrb_str_new(mrb, zchunk_data(result), zchunk_size(result));
      return mrb_funcall(mrb, mrb_obj_value(mrb_module_get(mrb, "MessagePack")), "unpack", 1, result_obj);
    } break;
    case MRB_ACTOR_MSG_ERROR: {
      const char* mrb_actor_class_cstr = mrb_actor_msg_mrb_class(actor_msg);
      mrb_value mrb_actor_class_str = mrb_str_new_static(mrb, mrb_actor_class_cstr, strlen(mrb_actor_class_cstr));
      mrb_value mrb_actor_class_obj = mrb_funcall(mrb,
          mrb_actor_class_str, "constantize", 0);
      mrb_raise(mrb, mrb_class_ptr(mrb_actor_class_obj), mrb_actor_msg_error(actor_msg));
    } break;
    default: {
      mrb_raise(mrb, E_ACTOR_PROTOCOL_ERROR, "Invalid Message recieved");
    }
    }
  }
  else
    mrb_sys_fail(mrb, "mrb_actor_send");

  return self;
}

static mrb_value
mrb_actor_async_send(mrb_state* mrb, mrb_value self)
{
  errno = 0;
  mrb_int object_id;
  mrb_sym method;
  mrb_value* argv;
  mrb_int argc = 0;

  mrb_get_args(mrb, "in*", &object_id, &method, &argv, &argc);

  mrb_value actor_msg_obj = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "actor_msg"));
  mrb_actor_msg_t* actor_msg = (mrb_actor_msg_t*)DATA_PTR(actor_msg_obj);
  mrb_actor_msg_set_id(actor_msg, MRB_ACTOR_MSG_ASYNC_SEND_MESSAGE);
  zuuid_t* uuid = zuuid_new();
  mrb_actor_msg_set_uuid(actor_msg, uuid);
  zuuid_destroy(&uuid);
  mrb_actor_msg_set_object_id(actor_msg, object_id);
  mrb_actor_msg_set_method(actor_msg, mrb_sym2name(mrb, method));

  if (argc > 0) {
    mrb_value args_ary = mrb_ary_new_from_values(mrb, argc, argv);
    mrb_value args_str = mrb_funcall(mrb, args_ary, "to_msgpack", 0);
    zchunk_t* args = zchunk_new(RSTRING_PTR(args_str), RSTRING_LEN(args_str));
    mrb_actor_msg_set_args(actor_msg, &args);
  }

  mrb_sym push_sym = mrb_intern_lit(mrb, "push");
  mrb_value push = mrb_iv_get(mrb, self, push_sym);
  if (mrb_actor_msg_send(actor_msg, (zsock_t*)DATA_PTR(push)) == 0) {
  }
  else
    mrb_sys_fail(mrb, "mrb_actor_async_send");

  return self;
}

static void
mrb_actor_msg_free(mrb_state* mrb, void* p)
{
  mrb_actor_msg_destroy((mrb_actor_msg_t**)&p);
}

static const struct mrb_data_type mrb_actor_msg_type = {
  "$i_mrb_actor_msg_type", mrb_actor_msg_free
};

static mrb_value
mrb_actor_msg_init(mrb_state* mrb, mrb_value self)
{
  errno = 0;
  mrb_actor_msg_t* actor_msg = mrb_actor_msg_new();

  if (actor_msg)
    mrb_data_init(self, actor_msg, &mrb_actor_msg_type);
  else if (errno == ENOMEM) {
    mrb->out_of_memory = TRUE;
    mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err));
  }
  else
    mrb_sys_fail(mrb, "mrb_actor_msg_init");

  return self;
}

void mrb_mruby_actor_gem_init(mrb_state* mrb)
{
  struct RClass *mrb_actor_class, *mrb_actor_msg_class;

  mrb_actor_class = mrb_define_class(mrb, "Actor", mrb->object_class);
  mrb_define_method(mrb, mrb_actor_class, "initialize", mrb_actor_initialize, (MRB_ARGS_REQ(1) | MRB_ARGS_REST()));
  mrb_define_method(mrb, mrb_actor_class, "init", mrb_actor_init, (MRB_ARGS_REQ(1) | MRB_ARGS_REST()));
  mrb_define_method(mrb, mrb_actor_class, "send", mrb_actor_send, (MRB_ARGS_REQ(2) | MRB_ARGS_REST()));
  mrb_define_method(mrb, mrb_actor_class, "async_send", mrb_actor_async_send, (MRB_ARGS_REQ(2) | MRB_ARGS_REST()));

  mrb_actor_msg_class = mrb_define_class_under(mrb, mrb_actor_class, "Msg", mrb->object_class);
  MRB_SET_INSTANCE_TT(mrb_actor_msg_class, MRB_TT_DATA);
  mrb_define_method(mrb, mrb_actor_msg_class, "initialize", mrb_actor_msg_init, MRB_ARGS_NONE());
}

void mrb_mruby_actor_gem_final(mrb_state* mrb)
{
}

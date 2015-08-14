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

typedef struct {
  char *mrb_file;
  FILE *mrb_file_handle;
  mrb_state *mrb;
  zloop_t *reactor;
  mrb_actor_msg_t *actor_msg;
  zsock_t *router;
  zsock_t *pull;
  zsock_t *pub;
} self_t;

static void
s_self_destroy(self_t **self_p)
{
  assert(self_p);

  if (*self_p) {
    self_t *self = *self_p;
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
mrb_actor_pipe_reader(zloop_z *reactor, zsock_t *pipe, void *args)
{
  int rc = 0;
  zmsg_t *msg = zmsg_recv(pipe);
  if (!msg)
    return -1;    // Interrupted

  char *command = zmsg_popstr (msg);
  printf("command: %s\n", command);
  if (streq(command, "$TERM"))
    rc = -1;
  else
  if (streq(command, "BIND ROUTER")) {
    char *endpoint = zmsg_popstr(msg);
    if (zsock_bind(self->router, "%s", endpoint) == -1) {
      zsys_warning ("could not bind router to %s (%s)", endpoint, zmq_strerror (zmq_errno ()));
      zsock_send(pipe, 1);
    }
    else
      zsock_send(pipe, 0);
    zstr_free(&endpoint);
  }
  else
  if (streq(command, "BIND PULL")) {
    char *endpoint = zmsg_popstr(msg);
    if (zsock_bind(self->pull, "%s", endpoint) == -1) {
      zsys_warning ("could not bind pull to %s (%s)", endpoint, zmq_strerror (zmq_errno ()));
      zsock_send(pipe, 1);
    }
    else
      zsock_send(pipe, 0);
    zstr_free(&endpoint);
  }
  else
  if (streq(command, "BIND PUB")) {
    char *endpoint = zmsg_popstr(msg);
    if (zsock_bind(self->pub, "%s", endpoint) == -1) {
      zsys_warning ("could not bind pub to %s (%s)", endpoint, zmq_strerror (zmq_errno ()));
      zsock_send(pipe, 1);
    }
    else
      zsock_send(pipe, 0);
    zstr_free(&endpoint);
  }

  zstr_free(&command);
  zmsg_destroy(&msg);

  return rc;
}

static int
mrb_actor_router_reader(zloop_z *reactor, zsock_t *router, void *args)
{
  self_t *self = (self_t *) args;
  mrb_state *mrb = self->mrb;
  int ai = mrb_gc_arena_save(mrb);

  int rc = mrb_actor_msg_recv(self->actor_msg, router);
  if (rc == -1)
    return rc;

  switch (mrb_actor_msg_id(self->actor_msg)) {
    case MRB_ACTOR_MSG_INITIALIZE: {
      const char *mrb_actor_class = mrb_actor_msg_class(self->actor_msg);
      zchunk_t *args = mrb_actor_msg_args(self->actor_msg);
      struct mrb_jmpbuf *prev_jmp = mrb->jmp;
      struct mrb_jmpbuf c_jmp;

      MRB_TRY(&c_jmp) {
        struct RClass *mrb_class = mrb_class_get(mrb, mrb_actor_class);
        mrb_value obj;
        if (zchunk_size(args) > 0) {
          mrb_value args_str = mrb_str_new_static(mrb, zchunk_data(args), zchunk_size(args));
          mrb_value args_obj = mrb_funcall(mrb, msgpack_mod, "unpack", 1, args_str);
          if (!mrb_ary_p(args_obj))
            mrb_raise(mrb, E_ARGUMENT_ERROR, "args must be a Array");
          obj = mrb_obj_new(mrb, mrb_class, RARRAY_LEN(args_obj), RARRAY_PTR(args_obj));
        }
        else
          obj = mrb_obj_new(mrb, mrb_class, 0, NULL);

        mrb_sym actor_state_sym = mrb_intern_lit(mrb, "mruby_actor_state");
        mrb_value actor_state = mrb_gv_get(mrb, actor_state_sym);
        mrb_int object_id = mrb_obj_id(obj);
        mrb_value object_id_val = mrb_fixnum_value(object_id);
        mrb_hash_set(mrb, actor_state, object_id_val, obj);
        mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_INITIALIZE_OK);
        mrb_actor_msg_set_object_id(self->actor_msg, object_id);
        mrb->jmp = prev_jmp;
      } MRB_CATCH(&c_jmp) {
        mrb->jmp = prev_jmp;
        mrb_value exc = mrb_obj_value(mrb->exc);
        mrb_value exc_str = mrb_funcall(mrb, exc, "to_s", 0);
        const char *exc_msg = mrb_string_value_cstr(mrb, exc_str);
        mrb_actor_msg_set_id(self->actor_msg, MRB_ACTOR_MSG_ERROR);
        mrb_actor_msg_set_mrb_class(self->actor_msg, mrb_class_name(mrb, mrb_class_ptr(mrb->exc)));
        mrb_actor_msg_set_error(self->actor_msg, exc_msg);
      } MRB_END_EXC(&c_jmp);
    }
    break;
    case MRB_ACTOR_MSG_SEND:
    default: {
      mrb_actor_set_msg_id(self->actor_msg, MRB_ACTOR_MSG_ERROR);
      mrb_actor_set_mrb_class(self->actor_msg, "Actor::ProtocolError");
      mrb_actor_set_error(self->actor_msg, "Invalid Message recieved");
    }
  }

  rc = mrb_actor_msg_send(self->actor_msg, router);
  mrb_gc_arena_restore(mrb, ai);

  return rc;
}

static int
mrb_actor_pull_reader(zloop_z *reactor, zsock_t *pull, void *args)
{
  self_t *self = (self_t *) args;
  mrb_state *mrb = self->mrb;

  int rc = mrb_actor_msg_recv(self->actor_msg, pull);

  switch (mrb_actor_msg_id(self->actor_msg)) {
    case MRB_ACTOR_MSG_ASYNC_SEND:
    default: {
      mrb_actor_set_msg_id(self->actor_msg, MRB_ACTOR_MSG_ASYNC_ERROR);
      mrb_actor_set_class(self->actor_msg, "Actor::ProtocolError");
      mrb_actor_set_error(self->actor_msg, "Invalid Message recieved");
    }
  }

  rc = mrb_actor_msg_send(self->actor_msg, self->pub);

  return rc;
}

static self_t *
s_self_new(zsock_t *pipe, const char *mrb_file)
{
  assert(pipe);
  assert(mrb_file);
  assert(strlen(mrb_file));

  int rc = -1;
  self_t *self = (self_t *) zmalloc (sizeof (self_t));
  if (!self)
      return NULL;

  self->mrb_file = strdup(mrb_file);
  if (self->mrb_file)
    self->mrb_file_handle = fopen(mrb_file, "r");
  if (self->mrb_file_handle)
    self->mrb = mrb_open();
  if (self->mrb) {
    mrb_state *mrb = self->mrb;
    struct mrb_jmpbuf *prev_jmp = mrb->jmp;
    struct mrb_jmpbuf c_jmp;
    MRB_TRY(&c_jmp) {
      mrb_value ret = mrb_load_file(mrb, mrb_file_handle);
      if (mrb_undef_p(ret))
        mrb_exc_raise(mrb, mrb->exc);
      mrb_sym actor_state_sym = mrb_intern_lit(mrb, "mruby_actor_state");
      mrb_value actor_state = mrb_hash_new(mrb);
      mrb_gv_set(mrb, actor_state_sym, actor_state);
      self->reactor = zloop_new();
      mrb->jmp = prev_jmp;
    } MRB_CATCH(&c_jmp) {
      mrb->jmp = prev_jmp;
      mrb_p(mrb, mrb_obj_value(mrb->exc));
    } MRB_END_EXC(&c_jmp);
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
mrb_zactor_fn(zsock_t *pipe, void *mrb_file)
{
  self_t *self = s_self_new(pipe, (const char *) mrb_file);
  assert(self);

  zsock_signal(pipe, 0);

  zloop_start(self->reactor);

  s_self_destroy(&self);
  zsock_signal(pipe, 0);
}

static void
mrb_actor_msg_free(mrb_state *mrb, void *p)
{
  mrb_actor_msg_destroy((mrb_actor_msg_t **) &p);
}

static const struct mrb_data_type mrb_actor_msg_type = {
  "$i_mrb_actor_msg_type", mrb_actor_msg_free
};

static mrb_value
mrb_actor_initalize(mrb_state *mrb, mrb_value self)
{
  errno = 0;
  char *mrb_file, *router_endpoint, *pull_endpoint, *pub_endpoint;

  mrb_get_args(mrb, "z|zzz", &mrb_file, &router_endpoint, &pull_endpoint, &pub_endpoint);

  zsock_t *dealer = zsock_new(ZMQ_DEALER);
  mrb_value dealer_sock = mrb_obj_value(mrb_data_object_alloc(mrb, mrb_class_ptr(self),
    dealer, &mrb_zsock_actor_type));
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "dealer"), dealer_sock);

  zsock_t *push = zsock_new(ZMQ_PUSH);
  mrb_value push_sock = mrb_obj_value(mrb_data_object_alloc(mrb, mrb_class_ptr(self),
    push, &mrb_zsock_actor_type));
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "push"), push_sock);

  zactor_t *zactor = zactor_new(mrb_zactor_fn, mrb_file);
  mrb_data_init(self, zactor, &mrb_zsock_actor_type);
  mrb_actor_msg_t *actor_msg = mrb_actor_msg_new();
  mrb_value actor_msg_obj = mrb_obj_value(mrb_data_object_alloc(mrb,
    mrb_class_get_under(mrb, mrb_class_ptr(self), "Msg"),
    actor_msg, &mrb_actor_msg_type));

  if (zactor && actor_msg) {
    mrb_sym actor_msg_sym = mrb_intern_lit(mrb, "actor_msg")
    mrb_iv_set(mrb, self, actor_msg_sym, actor_msg_obj);
    if (router_endpoint) {
      if (zsock_send(zactor, "ss", "BIND ROUTER", router_endpoint) == 0 && zsock_wait(zactor) == 0)
        zsock_connect(dealer, "%s", router_endpoint);
      else
        mrb_sys_fail(mrb, "zactor_new_mrb_zactor_fn");
    }
    if (pull_endpoint) {
      if (zsock_send(zactor, "ss", "BIND PULL", pull_endpoint) == 0 && zsock_wait(zactor) == 0)
        zsock_connect(push, "%s", pull_endpoint);
      else
        mrb_sys_fail(mrb, "zactor_new_mrb_zactor_fn");
    }
    if (pub_endpoint) {
      if (zsock_send(zactor, "ss", "BIND PUB", pub_endpoint) == 0 && zsock_wait(zactor) == 0) {

      }
      else
        mrb_sys_fail(mrb, "zactor_new_mrb_zactor_fn");
    }
  }
  else
  if (errno == ENOMEM) {
    mrb->out_of_memory = TRUE;
    mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err));
  }
  else
    mrb_sys_fail(mrb, "zactor_new_mrb_zactor_fn");

  return self;
}

static mrb_value
mrb_actor_init(mrb_state *mrb, mrb_value self)
{
  errno = 0;
  char *mrb_actor_class;
  mrb_value *argv;
  mrb_int argc = 0;

  mrb_get_args(mrb, "z&", &mrb_actor_class, &argv, &argc);

  mrb_value actor_msg_obj = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "actor_msg"));
  mrb_actor_msg_t *actor_msg = (mrb_actor_msg_t *) DATA_PTR(actor_msg_obj);
  mrb_actor_msg_set_mrb_class(actor_msg, mrb_actor_class);

  if (argc > 0) {
    mrb_value args_ary = mrb_ary_new_from_values(mrb, argc, argv);
    mrb_value args_str = mrb_funcall(mrb, args_ary, "to_msgpack", 0, NULL);
    zchunk_t *args = zchunk_new(RSTRING_PTR(args_str), RSTRING_LEN(args_str));
    mrb_actor_msg_set_args(actor_msg, &args);
  }

  mrb_actor_msg_set_id(actor_msg, MRB_ACTOR_MSG_INITIALIZE);

  mrb_sym dealer_sym = mrb_intern_lit(mrb, "dealer");
  mrb_value dealer_sock = mrb_iv_get(mrb, self, dealer_sym);
  if (mrb_actor_msg_send(actor_msg, (zsock_t *) DATA_PTR(dealer_sock)) == 0 &&
    mrb_actor_msg_recv(actor_msg, (zsock_t *) DATA_PTR(dealer_sock) == 0) {
  }
  else
    mrb_sys_fail(mrb, "mrb_actor_init");

  return self;
}

void
mrb_mruby_actor_gem_init(mrb_state *mrb) {
  struct RClass *mrb_actor_class, *mrb_actor_msg_class;

  mrb_actor_class = mrb_define_class(mrb, "Actor", mrb_class_get_under(mrb, mrb_module_get(mrb, "CZMQ"), "ZsockActor"));
  mrb_define_method(mrb, mrb_actor_class, "initialize", mrb_actor_initalize, MRB_ARGS_REQ(1));

  mrb_actor_msg_class = mrb_define_class_under(mrb, mrb_actor_class, "Msg", mrb->object_class);
  MRB_SET_INSTANCE_TT(mrb_actor_msg_class, MRB_TT_DATA);
}

void
mrb_mruby_actor_gem_final(mrb_state* mrb) {
}

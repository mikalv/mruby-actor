#include <stdio.h>
#include <mruby.h>
#include <czmq.h>
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
#include "./mrb_actor_message_body.h"

typedef struct {
    char* mrb_file;
    FILE* mrb_file_handle;
    mrb_state* mrb;
    zloop_t* reactor;
    actor_message_t* actor_msg;
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
        actor_message_destroy(&self->actor_msg);
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
        else {
            zsock_signal(pipe, 0);
            zsock_send(pipe, "s", zsock_endpoint(self->router));
        }
        zstr_free(&endpoint);
    }
    else if (streq(command, "BIND PULL")) {
        char* endpoint = zmsg_popstr(msg);
        if (zsock_bind(self->pull, "%s", endpoint) == -1) {
            zsys_warning("could not bind pull to %s (%s)", endpoint, zmq_strerror(zmq_errno()));
            zsock_signal(pipe, 1);
        }
        else {
            zsock_signal(pipe, 0);
            zsock_send(pipe, "s", zsock_endpoint(self->pull));
        }
        zstr_free(&endpoint);
    }
    else if (streq(command, "BIND PUB")) {
        char* endpoint = zmsg_popstr(msg);
        if (zsock_bind(self->pub, "%s", endpoint) == -1) {
            zsys_warning("could not bind pub to %s (%s)", endpoint, zmq_strerror(zmq_errno()));
            zsock_signal(pipe, 1);
        }
        else {
            zsock_signal(pipe, 0);
            zsock_send(pipe, "s", zsock_endpoint(self->pub));
        }
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

    int rc = actor_message_recv(self->actor_msg, router);
    if (rc == -1)
        return rc;

    switch (actor_message_id(self->actor_msg)) {
    case ACTOR_MESSAGE_INITIALIZE: {
        const char* mrb_actor_class_cstr = actor_message_mrb_class(self->actor_msg);
        zchunk_t* args = actor_message_args(self->actor_msg);
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
            actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_INITIALIZE_OK);
            actor_message_set_object_id(self->actor_msg, object_id);
            mrb->jmp = prev_jmp;
        }
        MRB_CATCH(&c_jmp)
        {
            mrb->jmp = prev_jmp;
            actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_ERROR);
            mrb_value exc = mrb_obj_value(mrb->exc);
            const char* classname = mrb_obj_classname(mrb, exc);
            actor_message_set_mrb_class(self->actor_msg, classname);
            mrb_value exc_str = mrb_funcall(mrb, exc, "to_s", 0);
            const char* exc_msg = mrb_string_value_cstr(mrb, &exc_str);
            actor_message_set_error(self->actor_msg, exc_msg);
            mrb->exc = NULL;
        }
        MRB_END_EXC(&c_jmp);
    } break;
    case ACTOR_MESSAGE_SEND_MESSAGE: {
        uint64_t object_id = actor_message_object_id(self->actor_msg);
        const char* method = actor_message_method(self->actor_msg);
        zchunk_t* args = actor_message_args(self->actor_msg);
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
            actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_SEND_OK);
            actor_message_set_result(self->actor_msg, &result_chunk);
        }
        MRB_CATCH(&c_jmp)
        {
            mrb->jmp = prev_jmp;
            actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_ERROR);
            mrb_value exc = mrb_obj_value(mrb->exc);
            const char* classname = mrb_obj_classname(mrb, exc);
            actor_message_set_mrb_class(self->actor_msg, classname);
            mrb_value exc_str = mrb_funcall(mrb, exc, "to_s", 0);
            const char* exc_msg = mrb_string_value_cstr(mrb, &exc_str);
            actor_message_set_error(self->actor_msg, exc_msg);
            mrb->exc = NULL;
        }
        MRB_END_EXC(&c_jmp);
    } break;
    default: {
        actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_ERROR);
        actor_message_set_mrb_class(self->actor_msg, "Actor::ProtocolError");
        actor_message_set_error(self->actor_msg, "Invalid Message recieved");
    }
    }

    rc = actor_message_send(self->actor_msg, router);
    mrb_gc_arena_restore(mrb, ai);

    return rc;
}

static int
mrb_actor_pull_reader(zloop_t* reactor, zsock_t* pull, void* args)
{
    self_t* self = (self_t*)args;
    mrb_state* mrb = self->mrb;
    int ai = mrb_gc_arena_save(mrb);

    int rc = actor_message_recv(self->actor_msg, pull);
    if (rc == -1)
        return rc;

    switch (actor_message_id(self->actor_msg)) {
    case ACTOR_MESSAGE_ASYNC_SEND_MESSAGE: {
        uint64_t object_id = actor_message_object_id(self->actor_msg);
        const char* method = actor_message_method(self->actor_msg);
        zchunk_t* args = actor_message_args(self->actor_msg);
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
            actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_ASYNC_SEND_OK);
            actor_message_set_result(self->actor_msg, &result_chunk);
        }
        MRB_CATCH(&c_jmp)
        {
            mrb->jmp = prev_jmp;
            actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_ASYNC_ERROR);
            mrb_value exc = mrb_obj_value(mrb->exc);
            const char* classname = mrb_obj_classname(mrb, exc);
            actor_message_set_mrb_class(self->actor_msg, classname);
            mrb_value exc_str = mrb_funcall(mrb, exc, "to_s", 0);
            const char* exc_msg = mrb_string_value_cstr(mrb, &exc_str);
            actor_message_set_error(self->actor_msg, exc_msg);
            mrb->exc = NULL;
        }
        MRB_END_EXC(&c_jmp);
    } break;
    default: {
        actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_ASYNC_ERROR);
        actor_message_set_mrb_class(self->actor_msg, "Actor::ProtocolError");
        actor_message_set_error(self->actor_msg, "Invalid Message recieved");
    }
    }

    rc = actor_message_send(self->actor_msg, self->pub);
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
            if (mrb->exc)
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
        self->actor_msg = actor_message_new();
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
    if (self->pub) {
        zsys_set_logident("mruby_actor");
    }
    else {
        s_self_destroy(&self);
    }

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
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@dealer"), dealer);

    mrb_value push_args[1];
    push_args[0] = mrb_fixnum_value(ZMQ_PUSH);
    mrb_value push = mrb_obj_new(mrb, mrb_class_get_under(mrb, mrb_module_get(mrb, "CZMQ"), "Zsock"), 1, push_args);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@push"), push);

    mrb_value actor_message_obj = mrb_obj_new(mrb, mrb_class_get(mrb, "ActorMessage"), 0, NULL);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@actor_message"), actor_message_obj);

    mrb_value actor_args[2];
    actor_args[0] = mrb_cptr_value(mrb, mrb_zactor_fn);
    actor_args[1] = mrb_file;

    mrb_value zactor = mrb_obj_new(mrb,
        mrb_class_get_under(mrb,
                                       mrb_module_get(mrb, "CZMQ"), "Zactor"),
        2, actor_args);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@zactor"), zactor);

    if (router_endpoint) {
        if (zsock_send(DATA_PTR(zactor), "ss", "BIND ROUTER", router_endpoint) == 0 && zsock_wait(DATA_PTR(zactor)) == 0) {
            char* endpoint;
            zsock_recv(DATA_PTR(zactor), "s", &endpoint);
            mrb_assert(endpoint);
            mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@router_endpoint"), mrb_str_new_cstr(mrb, endpoint));
            if (zsock_connect(DATA_PTR(dealer), "%s", endpoint) == -1) {
                zsys_warning("could not connect dealer to %s", endpoint);
                zstr_free(&endpoint);
                mrb_sys_fail(mrb, "zsock_connect");
            }
            zstr_free(&endpoint);
        }
        else
            mrb_sys_fail(mrb, "zsock_send");
    }
    if (pull_endpoint) {
        if (zsock_send(DATA_PTR(zactor), "ss", "BIND PULL", pull_endpoint) == 0 && zsock_wait(DATA_PTR(zactor)) == 0) {
            char* endpoint;
            zsock_recv(DATA_PTR(zactor), "s", &endpoint);
            mrb_assert(endpoint);
            mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@pull_endpoint"), mrb_str_new_cstr(mrb, endpoint));
            if (zsock_connect(DATA_PTR(push), "%s", endpoint) == -1) {
                zsys_warning("could not connect pull to %s", endpoint);
                zstr_free(&endpoint);
                mrb_sys_fail(mrb, "zsock_connect");
            }
            zstr_free(&endpoint);
        }
        else
            mrb_sys_fail(mrb, "zsock_send");
    }
    if (pub_endpoint) {
        if (zsock_send(DATA_PTR(zactor), "ss", "BIND PUB", pub_endpoint) == 0 && zsock_wait(DATA_PTR(zactor)) == 0) {
            char* endpoint;
            zsock_recv(DATA_PTR(zactor), "s", &endpoint);
            mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@pub_endpoint"), mrb_str_new_cstr(mrb, endpoint));
            zstr_free(&endpoint);
        }
        else
            mrb_sys_fail(mrb, "zsock_send");
    }

    return self;
}

void mrb_mruby_actor_gem_init(mrb_state* mrb)
{
    struct RClass* mrb_actor_class;

    mrb_actor_class = mrb_define_class(mrb, "Actor", mrb->object_class);
    mrb_define_method(mrb, mrb_actor_class, "initialize", mrb_actor_initialize, (MRB_ARGS_REQ(1) | MRB_ARGS_REST()));

#include "./mrb_actor_message_gem_init.h"
}

void mrb_mruby_actor_gem_final(mrb_state* mrb)
{
}

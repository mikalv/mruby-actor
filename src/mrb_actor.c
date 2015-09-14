#include "./mrb_actor_message_body.h"
#include "actor_discovery.h"
#include <mruby/dump.h>
#include <mruby/variable.h>
#include <zyre.h>

typedef struct {
    mrb_state* mrb;
    zloop_t* reactor;
    actor_message_t* actor_msg;
    zsock_t* router;
    zsock_t* pull;
    zsock_t* pub;
    zyre_t* discovery;
    actor_discovery_t* actor_discovery_msg;
} self_t;

static void
s_self_destroy(self_t** self_p)
{
    assert(self_p);

    if (*self_p) {
        self_t* self = *self_p;
        if (self->mrb) {
            mrb_close(self->mrb);
            self->mrb = NULL;
        }
        zloop_destroy(&self->reactor);
        actor_message_destroy(&self->actor_msg);
        zsock_destroy(&self->router);
        zsock_destroy(&self->pull);
        zsock_destroy(&self->pub);
        if (self->discovery) {
            zyre_stop(self->discovery);
            zclock_sleep(100);
            zyre_destroy(&self->discovery);
        }
        actor_discovery_destroy(&self->actor_discovery_msg);
        free(self);
        *self_p = NULL;
    }
}

static int
mrb_actor_pipe_reader(zloop_t* reactor, zsock_t* pipe, void* args)
{
    errno = 0;
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
            zsock_signal(pipe, 1);
            zsock_send(pipe, "i", errno);
        }
        else {
            const char* boundendpoint = zsock_endpoint(self->router);
            zyre_set_header(self->discovery, "mrb-actor-v1-router", "%s", boundendpoint);
            zsock_signal(pipe, 0);
            zsock_send(pipe, "s", boundendpoint);
        }
        zstr_free(&endpoint);
    }
    else if (streq(command, "BIND PULL")) {
        char* endpoint = zmsg_popstr(msg);
        if (zsock_bind(self->pull, "%s", endpoint) == -1) {
            zsock_signal(pipe, 1);
            zsock_send(pipe, "i", errno);
        }
        else {
            const char* boundendpoint = zsock_endpoint(self->pull);
            zyre_set_header(self->discovery, "mrb-actor-v1-pull", "%s", boundendpoint);
            zsock_signal(pipe, 0);
            zsock_send(pipe, "s", boundendpoint);
        }
        zstr_free(&endpoint);
    }
    else if (streq(command, "BIND PUB")) {
        char* endpoint = zmsg_popstr(msg);
        if (zsock_bind(self->pub, "%s", endpoint) == -1) {
            zsock_signal(pipe, 1);
            zsock_send(pipe, "i", errno);
        }
        else {
            const char* boundendpoint = zsock_endpoint(self->pub);
            zyre_set_header(self->discovery, "mrb-actor-v1-pub", "%s", boundendpoint);
            zsock_signal(pipe, 0);
            zsock_send(pipe, "s", boundendpoint);
        }
        zstr_free(&endpoint);
    }
    else if (streq(command, "ZYRE SET ENDPOINT")) {
        char* endpoint = zmsg_popstr(msg);
        if (zyre_set_endpoint(self->discovery, "%s", endpoint) == -1) {
            zsock_signal(pipe, 1);
        }
        else {
            zsock_signal(pipe, 0);
        }
        zstr_free(&endpoint);
    }
    else if (streq(command, "ZYRE GOSSIP BIND")) {
        char* endpoint = zmsg_popstr(msg);
        zyre_gossip_bind(self->discovery, "%s", endpoint);
        zstr_free(&endpoint);
    }
    else if (streq(command, "ZYRE GOSSIP CONNECT")) {
        char* endpoint = zmsg_popstr(msg);
        zyre_gossip_connect(self->discovery, "%s", endpoint);
        zstr_free(&endpoint);
    }
    else if (streq(command, "ZYRE START")) {
        if (zyre_start(self->discovery) == -1) {
            zsock_signal(pipe, 1);
        }
        else {
            zyre_join(self->discovery, "mrb-actor-v1");
            zsock_signal(pipe, 0);
        }
    }
    else if (streq(command, "LOAD IREP FILE")) {
        char* mrb_file = zmsg_popstr(msg);
        mrb_state* mrb = self->mrb;
        int ai = mrb_gc_arena_save(mrb);
        struct mrb_jmpbuf* prev_jmp = mrb->jmp;
        struct mrb_jmpbuf c_jmp;

        MRB_TRY(&c_jmp)
        {
            mrb->jmp = &c_jmp;
            FILE* fp = fopen(mrb_file, "rb");
            if (!fp) {
                mrb_sys_fail(mrb, "fopen");
            }
            mrb_load_irep_file(mrb, fp);
            fclose(fp);
            if (mrb->exc) {
                mrb_exc_raise(mrb, mrb_obj_value(mrb->exc));
            }
            zsock_signal(pipe, 0);
            mrb->jmp = prev_jmp;
        }
        MRB_CATCH(&c_jmp)
        {
            mrb->jmp = prev_jmp;
            mrb_p(mrb, mrb_obj_value(mrb->exc));
            zsock_signal(pipe, 1);
            mrb->exc = NULL;
        }
        MRB_END_EXC(&c_jmp);
        mrb_gc_arena_restore(mrb, ai);

        zstr_free(&mrb_file);
    }
    else if (streq(command, "GET REMOTE ACTORS")) {
        mrb_state* mrb = self->mrb;
        int ai = mrb_gc_arena_save(mrb);
        struct mrb_jmpbuf* prev_jmp = mrb->jmp;
        struct mrb_jmpbuf c_jmp;

        MRB_TRY(&c_jmp)
        {
            mrb->jmp = &c_jmp;
            mrb_sym remote_actor_state_sym = mrb_intern_lit(mrb, "mruby_remote_actor_state");
            mrb_value remote_actor_state = mrb_gv_get(mrb, remote_actor_state_sym);
            mrb_value remote_actor_state_msgpack = mrb_funcall(mrb, remote_actor_state, "to_msgpack", 0);
            zsock_signal(pipe, 0);
            zsock_send(pipe, "b", RSTRING_PTR(remote_actor_state_msgpack), RSTRING_LEN(remote_actor_state_msgpack));
            mrb->jmp = prev_jmp;
        }
        MRB_CATCH(&c_jmp)
        {
            mrb->jmp = prev_jmp;
            mrb_p(mrb, mrb_obj_value(mrb->exc));
            zsock_signal(pipe, 1);
            mrb->exc = NULL;
        }
        MRB_END_EXC(&c_jmp);
        mrb_gc_arena_restore(mrb, ai);
    }
    else {
        zsys_error("invalid command: %s", command);
        assert(false);
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
            mrb_value args_str = mrb_str_new_static(mrb, zchunk_data(args), zchunk_size(args));
            mrb_value args_obj = mrb_funcall(mrb, mrb_obj_value(mrb_module_get(mrb, "MessagePack")), "unpack", 1, args_str);
            if (!mrb_array_p(args_obj))
                mrb_raise(mrb, E_ARGUMENT_ERROR, "args must be a Array");
            mrb_value obj = mrb_obj_new(mrb, mrb_actor_class_ptr, RARRAY_LEN(args_obj), RARRAY_PTR(args_obj));
            mrb_sym actor_state_sym = mrb_intern_lit(mrb, "mruby_actor_state");
            mrb_value actor_state = mrb_gv_get(mrb, actor_state_sym);
            mrb_int object_id = mrb_obj_id(obj);
            mrb_value object_id_val = mrb_fixnum_value(object_id);
            mrb_hash_set(mrb, actor_state, object_id_val, obj);
            actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_INITIALIZE_OK);
            actor_message_set_object_id(self->actor_msg, object_id);
            zmsg_t* discovery_msg = zmsg_new();
            actor_discovery_set_id(self->actor_discovery_msg, ACTOR_DISCOVERY_OBJECT_NEW);
            actor_discovery_set_mrb_class(self->actor_discovery_msg, mrb_actor_class_cstr);
            actor_discovery_set_object_id(self->actor_discovery_msg, object_id);
            actor_discovery_send(self->actor_discovery_msg, discovery_msg);
            zyre_shout(self->discovery, "mrb-actor-v1", &discovery_msg);
            zmsg_destroy(&discovery_msg);
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
            if (mrb_nil_p(obj))
                mrb_raise(mrb, E_KEY_ERROR, "key not found");
            mrb_value args_str = mrb_str_new_static(mrb, zchunk_data(args), zchunk_size(args));
            mrb_value args_obj = mrb_funcall(mrb, mrb_obj_value(mrb_module_get(mrb, "MessagePack")), "unpack", 1, args_str);
            if (!mrb_array_p(args_obj))
                mrb_raise(mrb, E_ARGUMENT_ERROR, "args must be a Array");
            mrb_sym method_sym = mrb_intern_cstr(mrb, method);
            mrb_value result = mrb_funcall_argv(mrb, obj, method_sym, RARRAY_LEN(args_obj), RARRAY_PTR(args_obj));
            mrb_value result_str = mrb_funcall(mrb, result, "to_msgpack", 0);
            zchunk_t* result_chunk = zchunk_new(RSTRING_PTR(result_str), RSTRING_LEN(result_str));
            actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_SEND_OK);
            actor_message_set_result(self->actor_msg, &result_chunk);
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
            mrb_value args_str = mrb_str_new_static(mrb, zchunk_data(args), zchunk_size(args));
            mrb_value args_obj = mrb_funcall(mrb, mrb_obj_value(mrb_module_get(mrb, "MessagePack")), "unpack", 1, args_str);
            if (!mrb_array_p(args_obj))
                mrb_raise(mrb, E_ARGUMENT_ERROR, "args must be a Array");
            mrb_sym method_sym = mrb_intern_cstr(mrb, method);
            mrb_value result = mrb_funcall_argv(mrb, obj, method_sym, RARRAY_LEN(args_obj), RARRAY_PTR(args_obj));
            mrb_value result_str = mrb_funcall(mrb, result, "to_msgpack", 0);
            zchunk_t* result_chunk = zchunk_new(RSTRING_PTR(result_str), RSTRING_LEN(result_str));
            actor_message_set_id(self->actor_msg, ACTOR_MESSAGE_ASYNC_SEND_OK);
            actor_message_set_result(self->actor_msg, &result_chunk);
            mrb->jmp = prev_jmp;
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

static int
mrb_actor_zyre_reader(zloop_t* reactor, zsock_t* pull, void* args)
{
    self_t* self = (self_t*)args;
    mrb_state* mrb = self->mrb;
    int ai = mrb_gc_arena_save(mrb);

    zyre_event_t* event = zyre_event_new(self->discovery);

    const char* name = zyre_event_name(event);
    const char* sender = zyre_event_sender(event);

    if (zyre_event_type(event) != ZYRE_EVENT_EVASIVE)
        zyre_event_print(event);

    switch (zyre_event_type(event)) {
    case ZYRE_EVENT_ENTER: {
        zhash_t* headers = zyre_event_headers(event);

        struct mrb_jmpbuf* prev_jmp = mrb->jmp;
        struct mrb_jmpbuf c_jmp;

        MRB_TRY(&c_jmp)
        {
            mrb->jmp = &c_jmp;
            mrb_sym remote_actor_state_sym = mrb_intern_lit(mrb, "mruby_remote_actor_state");
            mrb_value remote_actor_state_hash = mrb_gv_get(mrb, remote_actor_state_sym);
            mrb_value remote_actor_hash = mrb_hash_new_capa(mrb, 3);
            mrb_value sender_str = mrb_str_new_cstr(mrb, sender);
            mrb_value headers_hash = mrb_hash_new_capa(mrb, zhash_size(headers));
            mrb_value objects_hash = mrb_hash_new(mrb);
            mrb_sym uuid_sym = mrb_intern_lit(mrb, "uuid");
            mrb_value uuid_val = mrb_symbol_value(uuid_sym);
            mrb_sym headers_sym = mrb_intern_lit(mrb, "headers");
            mrb_value headers_val = mrb_symbol_value(headers_sym);
            mrb_sym objects_sym = mrb_intern_lit(mrb, "objects");
            mrb_value objects_val = mrb_symbol_value(objects_sym);
            mrb_value name_str = mrb_str_new_cstr(mrb, name);

            mrb_hash_set(mrb, remote_actor_hash, uuid_val, sender_str);
            mrb_hash_set(mrb, remote_actor_hash, headers_val, headers_hash);
            mrb_hash_set(mrb, remote_actor_hash, objects_val, objects_hash);
            mrb_hash_set(mrb, remote_actor_state_hash, name_str, remote_actor_hash);

            const char* value = (const char*)zhash_first(headers);
            while (value) {
                const char* key = zhash_cursor(headers);
                mrb_value key_str = mrb_str_new_cstr(mrb, key);
                mrb_value value_str = mrb_str_new_cstr(mrb, value);
                mrb_hash_set(mrb, headers_hash, key_str, value_str);
                mrb_gc_arena_restore(mrb, ai);
                value = (const char*)zhash_next(headers);
            }
            mrb->jmp = prev_jmp;
        }
        MRB_CATCH(&c_jmp)
        {
            mrb->jmp = prev_jmp;
            mrb_p(mrb, mrb_obj_value(mrb->exc));
            mrb->exc = NULL;
        }
        MRB_END_EXC(&c_jmp);
    } break;
    case ZYRE_EVENT_JOIN: {

    } break;
    case ZYRE_EVENT_LEAVE: {

    } break;
    case ZYRE_EVENT_EXIT: {
        struct mrb_jmpbuf* prev_jmp = mrb->jmp;
        struct mrb_jmpbuf c_jmp;

        MRB_TRY(&c_jmp)
        {
            mrb->jmp = &c_jmp;
            mrb_sym remote_actor_state_sym = mrb_intern_lit(mrb, "mruby_remote_actor_state");
            mrb_value remote_actor_state_hash = mrb_gv_get(mrb, remote_actor_state_sym);

            mrb_value name_str = mrb_str_new_static(mrb, name, strlen(name));

            mrb_hash_delete_key(mrb, remote_actor_state_hash, name_str);
            mrb->jmp = prev_jmp;
        }
        MRB_CATCH(&c_jmp)
        {
            mrb->jmp = prev_jmp;
            mrb_p(mrb, mrb_obj_value(mrb->exc));
            mrb->exc = NULL;
        }
        MRB_END_EXC(&c_jmp);


    } break;
    case ZYRE_EVENT_WHISPER: {

    } break;
    case ZYRE_EVENT_SHOUT: {
        zmsg_t* msg = zyre_event_msg(event);
        actor_discovery_recv(self->actor_discovery_msg, msg);
        switch (actor_discovery_id(self->actor_discovery_msg)) {
        case ACTOR_DISCOVERY_OBJECT_NEW: {
            struct mrb_jmpbuf* prev_jmp = mrb->jmp;
            struct mrb_jmpbuf c_jmp;

            MRB_TRY(&c_jmp)
            {
                mrb->jmp = &c_jmp;
                mrb_sym remote_actor_state_sym = mrb_intern_lit(mrb, "mruby_remote_actor_state");
                mrb_value remote_actor_state = mrb_gv_get(mrb, remote_actor_state_sym);
                mrb_value name_str = mrb_str_new_static(mrb, name, strlen(name));
                mrb_value remote_actor = mrb_hash_get(mrb, remote_actor_state, name_str);
                mrb_sym objects_sym = mrb_intern_lit(mrb, "objects");
                mrb_value objects_val = mrb_symbol_value(objects_sym);
                mrb_value remote_actor_objects = mrb_hash_get(mrb, remote_actor, objects_val);
                const char* mrb_class = actor_discovery_mrb_class(self->actor_discovery_msg);
                mrb_value mrb_class_str = mrb_str_new_static(mrb, mrb_class, strlen(mrb_class));
                mrb_value mrb_class_objs = mrb_hash_get(mrb, remote_actor_objects, mrb_class_str);
                if (mrb_nil_p(mrb_class_objs)) {
                    mrb_class_objs = mrb_ary_new_capa(mrb, 1);
                    mrb_hash_set(mrb, remote_actor_objects, mrb_class_str, mrb_class_objs);
                }

                uint64_t object_id = actor_discovery_object_id(self->actor_discovery_msg);
                mrb_value object_id_val = mrb_fixnum_value(object_id);
                mrb_ary_push(mrb, mrb_class_objs, object_id_val);
                mrb->jmp = prev_jmp;
            }
            MRB_CATCH(&c_jmp)
            {
                mrb->jmp = prev_jmp;
                mrb_p(mrb, mrb_obj_value(mrb->exc));
                mrb->exc = NULL;
            }
            MRB_END_EXC(&c_jmp);
        } break;
        default: {
        }
        }
    } break;
    case ZYRE_EVENT_STOP: {

    } break;
    case ZYRE_EVENT_EVASIVE: {

    } break;
    default: {
    }
    }

    zyre_event_destroy(&event);

    mrb_gc_arena_restore(mrb, ai);

    return 0;
}

static self_t*
s_self_new(zsock_t* pipe, const char* name)
{
    assert(pipe);
    assert(zsock_is(pipe));
    assert(name);
    assert(strlen(name) > 0);

    int rc = -1;
    self_t* self = (self_t*)zmalloc(sizeof(self_t));
    if (!self)
        return NULL;

    self->mrb = mrb_open();
    if (self->mrb) {
        mrb_state* mrb = self->mrb;
        int ai = mrb_gc_arena_save(mrb);
        struct mrb_jmpbuf* prev_jmp = mrb->jmp;
        struct mrb_jmpbuf c_jmp;

        MRB_TRY(&c_jmp)
        {
            mrb->jmp = &c_jmp;
            mrb_sym actor_state_sym = mrb_intern_lit(mrb, "mruby_actor_state");
            mrb_value actor_state = mrb_hash_new(mrb);
            mrb_gv_set(mrb, actor_state_sym, actor_state);
            mrb_sym remote_actor_state_sym = mrb_intern_lit(mrb, "mruby_remote_actor_state");
            mrb_value remote_actor_state = mrb_hash_new(mrb);
            mrb_gv_set(mrb, remote_actor_state_sym, remote_actor_state);
            self->reactor = zloop_new();
            mrb->jmp = prev_jmp;
        }
        MRB_CATCH(&c_jmp)
        {
            mrb->jmp = prev_jmp;
            mrb_p(mrb, mrb_obj_value(mrb->exc));
        }
        MRB_END_EXC(&c_jmp);

        mrb_gc_arena_restore(mrb, ai);
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
    if (self->pub)
        self->discovery = zyre_new(name);
    if (self->discovery)
        rc = zloop_reader(self->reactor, zyre_socket(self->discovery), mrb_actor_zyre_reader, self);
    if (rc == 0)
        self->actor_discovery_msg = actor_discovery_new();
    if (rc == -1 || !self->actor_discovery_msg)
        s_self_destroy(&self);

    return self;
}

static void
mrb_zactor_fn(zsock_t* pipe, void* name)
{
    self_t* self = s_self_new(pipe, (const char*)name);
    assert(self);

    zsock_signal(pipe, 0);

    zloop_start(self->reactor);

    s_self_destroy(&self);
    zsock_signal(pipe, 0);
}

static mrb_value
mrb_actor_gen_sub(mrb_state* mrb, mrb_value self)
{
    mrb_int signature, id, object_id;

    mrb_get_args(mrb, "iii", &signature, &id, &object_id);

    mrb_value sub = mrb_str_new(mrb, NULL, 2 + 1 + 8);
    byte* subscribe = (byte*)RSTRING_PTR(sub);
    *subscribe++ = (byte)(((0xAAA0 | signature) >> 8) & 255);
    *subscribe++ = (byte)(((0xAAA0 | signature)) & 255);
    *subscribe++ = (byte)(((id)) & 255);
    *subscribe++ = (byte)(((object_id) >> 56) & 255);
    *subscribe++ = (byte)(((object_id) >> 48) & 255);
    *subscribe++ = (byte)(((object_id) >> 40) & 255);
    *subscribe++ = (byte)(((object_id) >> 32) & 255);
    *subscribe++ = (byte)(((object_id) >> 24) & 255);
    *subscribe++ = (byte)(((object_id) >> 16) & 255);
    *subscribe++ = (byte)(((object_id) >> 8) & 255);
    *subscribe++ = (byte)(((object_id)) & 255);

    return sub;
}

void mrb_mruby_actor_gem_init(mrb_state* mrb)
{
    struct RClass* mrb_actor_class;

    mrb_actor_class = mrb_define_class(mrb, "Actor", mrb->object_class);
    mrb_define_const(mrb, mrb_actor_class, "ZACTOR_FN", mrb_cptr_value(mrb, mrb_zactor_fn));
    mrb_define_method(mrb, mrb_actor_class, "gen_sub", mrb_actor_gen_sub, MRB_ARGS_REQ(3));
    mrb_intern_lit(mrb, "uuid");
    mrb_intern_lit(mrb, "headers");
    mrb_intern_lit(mrb, "objects");

#include "./mrb_actor_message_gem_init.h"
}

void mrb_mruby_actor_gem_final(mrb_state* mrb)
{
}

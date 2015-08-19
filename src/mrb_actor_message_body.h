/*  =========================================================================
    actor_message - mruby actor messages

    Codec class for actor_message.

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: mruby_actor_message.xml, or
     * The code generation script that built this file: zproto_codec_mruby
    ************************************************************************
    =========================================================================
*/

/*
@header
    actor_message - mruby actor messages
@discuss
@end
*/

#include "./actor_message.h"
#include <errno.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/error.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/throw.h>

static void
mrb_actor_message_destroy (mrb_state *mrb, void *p)
{
    actor_message_destroy ((actor_message_t **) &p);
}

static const struct mrb_data_type mrb_actor_message_type = {
    "i_actor_message_type", mrb_actor_message_destroy
};

//  --------------------------------------------------------------------------
//  Create a new actor_message

static mrb_value
mrb_actor_message_initialize (mrb_state *mrb, mrb_value mrb_self)
{
    errno = 0;
    actor_message_t *self = actor_message_new ();
    if (self == NULL) {
        mrb_sys_fail (mrb, "actor_message_new");
    }

    mrb_data_init (mrb_self, self, &mrb_actor_message_type);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Receive a actor_message from the socket. Returns self if OK, else raises an exception.
//  Blocks if there is no message waiting.

static mrb_value
mrb_actor_message_recv (mrb_state *mrb, mrb_value mrb_self)
{
    errno = 0;
    mrb_value input_obj;

    mrb_get_args (mrb, "o", &input_obj);

    if (mrb_type (input_obj) != MRB_TT_DATA)
        mrb_raise(mrb, E_ARGUMENT_ERROR, "input is not a data type");

    mrb_assert (zsock_is (DATA_PTR (input_obj)));
    zsock_t *input = (zsock_t *) DATA_PTR (input_obj);

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    if (actor_message_recv (self, input) == -1) {
        mrb_sys_fail (mrb, "actor_message_recv");
    }

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Send the actor_message to the socket. Does not destroy it. Returns self if
//  OK, else raises an exception.

static mrb_value
mrb_actor_message_send (mrb_state *mrb, mrb_value mrb_self)
{
    errno = 0;
    mrb_value output_obj;

    mrb_get_args (mrb, "o", &output_obj);

    if (mrb_type (output_obj) != MRB_TT_DATA)
        mrb_raise(mrb, E_ARGUMENT_ERROR, "output is not a data type");

    mrb_assert (zsock_is (DATA_PTR (output_obj)));
    zsock_t *output = (zsock_t *) DATA_PTR (output_obj);

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    if (actor_message_send (self, output) == -1) {
        mrb_sys_fail (mrb, "actor_message_send");
    }

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Print contents of message to stdout

static mrb_value
mrb_actor_message_print (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    actor_message_print (self);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Get the message routing_id

static mrb_value
mrb_actor_message_routing_id (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    zframe_t *routing_id = actor_message_routing_id (self);

    return mrb_str_new_static (mrb, zframe_data (routing_id), zframe_size (routing_id));
}

//  Set the message routing_id

static mrb_value
mrb_actor_message_set_routing_id (mrb_state *mrb, mrb_value mrb_self)
{
    char *routing_id_str;
    mrb_int routing_id_len;

    mrb_get_args (mrb, "s", &routing_id_str, &routing_id_len);

    if (routing_id_len > 256) {
        mrb_raise (mrb, E_RANGE_ERROR, "routing_id is too large");
    }

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    zframe_t *routing_id = zframe_new (routing_id_str, routing_id_len);

    actor_message_set_routing_id (self, routing_id);

    zframe_destroy (&routing_id);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Get the actor_message id

static mrb_value
mrb_actor_message_id (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    // TODO: add bounds checking

    return mrb_fixnum_value (actor_message_id (self));
}

// Set the actor_message id

static mrb_value
mrb_actor_message_set_id (mrb_state *mrb, mrb_value mrb_self)
{
    mrb_int id;

    mrb_get_args (mrb, "i", &id);

    if (id < INT_MIN || id > INT_MAX)
        mrb_raise (mrb, E_RANGE_ERROR, "id is out of range");

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    actor_message_set_id (self, id);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Return a printable command string

static mrb_value
mrb_actor_message_command (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    const char *command = actor_message_command (self);

    return mrb_str_new_static (mrb, command, strlen (command));
}

//  --------------------------------------------------------------------------
//  Get the mrb_class field

static mrb_value
mrb_actor_message_mrb_class (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    const char *mrb_class = actor_message_mrb_class (self);

    return mrb_str_new_static (mrb, mrb_class, strlen (mrb_class));
}

// Set the mrb_class field

static mrb_value
mrb_actor_message_set_mrb_class (mrb_state *mrb, mrb_value mrb_self)
{
    char *mrb_class;

    mrb_get_args (mrb, "z", &mrb_class);

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    actor_message_set_mrb_class (self, mrb_class);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Get the args field

static mrb_value
mrb_actor_message_args (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    zchunk_t *args = actor_message_args (self);

    return mrb_str_new_static (mrb, zchunk_data (args), zchunk_size (args));
}

//  Set the args field

static mrb_value
mrb_actor_message_set_args (mrb_state *mrb, mrb_value mrb_self)
{
    char *args_str;
    mrb_int args_len;

    mrb_get_args (mrb, "s", &args_str, &args_len);

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    zchunk_t *args = zchunk_new (args_str, args_len);

    actor_message_set_args (self, &args);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Get the object_id field

static mrb_value
mrb_actor_message_object_id (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    // TODO: add bounds checking

    return mrb_fixnum_value (actor_message_object_id (self));
}

// Set the object_id field

static mrb_value
mrb_actor_message_set_object_id (mrb_state *mrb, mrb_value mrb_self)
{
    mrb_int object_id;

    mrb_get_args (mrb, "i", &object_id);

    // TODO: add bounds checking

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    actor_message_set_object_id (self, object_id);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Get the method field

static mrb_value
mrb_actor_message_method (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    const char *method = actor_message_method (self);

    return mrb_str_new_static (mrb, method, strlen (method));
}

// Set the method field

static mrb_value
mrb_actor_message_set_method (mrb_state *mrb, mrb_value mrb_self)
{
    char *method;

    mrb_get_args (mrb, "z", &method);

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    actor_message_set_method (self, method);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Get the result field

static mrb_value
mrb_actor_message_result (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    zchunk_t *result = actor_message_result (self);

    return mrb_str_new_static (mrb, zchunk_data (result), zchunk_size (result));
}

//  Set the result field

static mrb_value
mrb_actor_message_set_result (mrb_state *mrb, mrb_value mrb_self)
{
    char *result_str;
    mrb_int result_len;

    mrb_get_args (mrb, "s", &result_str, &result_len);

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    zchunk_t *result = zchunk_new (result_str, result_len);

    actor_message_set_result (self, &result);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Get the uuid field

static mrb_value
mrb_actor_message_uuid (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    zuuid_t *uuid = actor_message_uuid (self);

    return mrb_str_new_static (mrb, zuuid_data (uuid), zuuid_size (uuid));
}

// Set the uuid field

static mrb_value
mrb_actor_message_set_uuid (mrb_state *mrb, mrb_value mrb_self)
{
    char *uuid_str;
    mrb_int uuid_len;

    mrb_get_args (mrb, "s", &uuid_str, &uuid_len);

    if (uuid_len != ZUUID_LEN)
        mrb_raisef (mrb, E_ARGUMENT_ERROR, "uuid must be %S bytes long", mrb_fixnum_value (ZUUID_LEN));

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    zuuid_t *uuid = zuuid_new_from (uuid_str);
    actor_message_set_uuid (self, uuid);
    zuuid_destroy (&uuid);

    return mrb_self;
}

// Create a uuid

static mrb_value
mrb_actor_message_create_uuid (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    zuuid_t *uuid = zuuid_new ();
    actor_message_set_uuid (self, uuid);
    zuuid_destroy (&uuid);

    return mrb_self;
}

//  --------------------------------------------------------------------------
//  Get the error field

static mrb_value
mrb_actor_message_error (mrb_state *mrb, mrb_value mrb_self)
{
    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    const char *error = actor_message_error (self);

    return mrb_str_new_static (mrb, error, strlen (error));
}

// Set the error field

static mrb_value
mrb_actor_message_set_error (mrb_state *mrb, mrb_value mrb_self)
{
    char *error;

    mrb_get_args (mrb, "z", &error);

    actor_message_t *self = (actor_message_t *) DATA_PTR (mrb_self);

    actor_message_set_error (self, error);

    return mrb_self;
}


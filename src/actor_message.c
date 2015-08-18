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

//  Structure of our class

struct _actor_message_t {
    zframe_t *routing_id;               //  Routing_id from ROUTER, if any
    int id;                             //  actor_message message ID
    byte *needle;                       //  Read/write pointer for serialization
    byte *ceiling;                      //  Valid upper limit for read pointer
    char mrb_class [256];               //  mrb_class
    zchunk_t *args;                     //  args
    uint64_t object_id;                 //  object_id
    char method [256];                  //  method
    zchunk_t *result;                   //  result
    zuuid_t *uuid;                      //  uuid
    char error [256];                   //  error
};

//  --------------------------------------------------------------------------
//  Network data encoding macros

//  Put a block of octets to the frame
#define PUT_OCTETS(host,size) { \
    memcpy (self->needle, (host), size); \
    self->needle += size; \
}

//  Get a block of octets from the frame
#define GET_OCTETS(host,size) { \
    if (self->needle + size > self->ceiling) { \
        zsys_warning ("actor_message: GET_OCTETS failed"); \
        goto malformed; \
    } \
    memcpy ((host), self->needle, size); \
    self->needle += size; \
}

//  Put a 1-byte number to the frame
#define PUT_NUMBER1(host) { \
    *(byte *) self->needle = (host); \
    self->needle++; \
}

//  Put a 2-byte number to the frame
#define PUT_NUMBER2(host) { \
    self->needle [0] = (byte) (((host) >> 8)  & 255); \
    self->needle [1] = (byte) (((host))       & 255); \
    self->needle += 2; \
}

//  Put a 4-byte number to the frame
#define PUT_NUMBER4(host) { \
    self->needle [0] = (byte) (((host) >> 24) & 255); \
    self->needle [1] = (byte) (((host) >> 16) & 255); \
    self->needle [2] = (byte) (((host) >> 8)  & 255); \
    self->needle [3] = (byte) (((host))       & 255); \
    self->needle += 4; \
}

//  Put a 8-byte number to the frame
#define PUT_NUMBER8(host) { \
    self->needle [0] = (byte) (((host) >> 56) & 255); \
    self->needle [1] = (byte) (((host) >> 48) & 255); \
    self->needle [2] = (byte) (((host) >> 40) & 255); \
    self->needle [3] = (byte) (((host) >> 32) & 255); \
    self->needle [4] = (byte) (((host) >> 24) & 255); \
    self->needle [5] = (byte) (((host) >> 16) & 255); \
    self->needle [6] = (byte) (((host) >> 8)  & 255); \
    self->needle [7] = (byte) (((host))       & 255); \
    self->needle += 8; \
}

//  Get a 1-byte number from the frame
#define GET_NUMBER1(host) { \
    if (self->needle + 1 > self->ceiling) { \
        zsys_warning ("actor_message: GET_NUMBER1 failed"); \
        goto malformed; \
    } \
    (host) = *(byte *) self->needle; \
    self->needle++; \
}

//  Get a 2-byte number from the frame
#define GET_NUMBER2(host) { \
    if (self->needle + 2 > self->ceiling) { \
        zsys_warning ("actor_message: GET_NUMBER2 failed"); \
        goto malformed; \
    } \
    (host) = ((uint16_t) (self->needle [0]) << 8) \
           +  (uint16_t) (self->needle [1]); \
    self->needle += 2; \
}

//  Get a 4-byte number from the frame
#define GET_NUMBER4(host) { \
    if (self->needle + 4 > self->ceiling) { \
        zsys_warning ("actor_message: GET_NUMBER4 failed"); \
        goto malformed; \
    } \
    (host) = ((uint32_t) (self->needle [0]) << 24) \
           + ((uint32_t) (self->needle [1]) << 16) \
           + ((uint32_t) (self->needle [2]) << 8) \
           +  (uint32_t) (self->needle [3]); \
    self->needle += 4; \
}

//  Get a 8-byte number from the frame
#define GET_NUMBER8(host) { \
    if (self->needle + 8 > self->ceiling) { \
        zsys_warning ("actor_message: GET_NUMBER8 failed"); \
        goto malformed; \
    } \
    (host) = ((uint64_t) (self->needle [0]) << 56) \
           + ((uint64_t) (self->needle [1]) << 48) \
           + ((uint64_t) (self->needle [2]) << 40) \
           + ((uint64_t) (self->needle [3]) << 32) \
           + ((uint64_t) (self->needle [4]) << 24) \
           + ((uint64_t) (self->needle [5]) << 16) \
           + ((uint64_t) (self->needle [6]) << 8) \
           +  (uint64_t) (self->needle [7]); \
    self->needle += 8; \
}

//  Put a string to the frame
#define PUT_STRING(host) { \
    size_t string_size = strlen (host); \
    PUT_NUMBER1 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
}

//  Get a string from the frame
#define GET_STRING(host) { \
    size_t string_size; \
    GET_NUMBER1 (string_size); \
    if (self->needle + string_size > (self->ceiling)) { \
        zsys_warning ("actor_message: GET_STRING failed"); \
        goto malformed; \
    } \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}

//  Put a long string to the frame
#define PUT_LONGSTR(host) { \
    size_t string_size = strlen (host); \
    PUT_NUMBER4 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
}

//  Get a long string from the frame
#define GET_LONGSTR(host) { \
    size_t string_size; \
    GET_NUMBER4 (string_size); \
    if (self->needle + string_size > (self->ceiling)) { \
        zsys_warning ("actor_message: GET_LONGSTR failed"); \
        goto malformed; \
    } \
    free ((host)); \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}


//  --------------------------------------------------------------------------
//  Create a new actor_message

actor_message_t *
actor_message_new (void)
{
    actor_message_t *self = (actor_message_t *) zmalloc (sizeof (actor_message_t));
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the actor_message

void
actor_message_destroy (actor_message_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        actor_message_t *self = *self_p;

        //  Free class properties
        zframe_destroy (&self->routing_id);
        zchunk_destroy (&self->args);
        zchunk_destroy (&self->result);
        zuuid_destroy (&self->uuid);

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Receive a actor_message from the socket. Returns 0 if OK, -1 if
//  there was an error. Blocks if there is no message waiting.

int
actor_message_recv (actor_message_t *self, zsock_t *input)
{
    assert (input);

    if (zsock_type (input) == ZMQ_ROUTER) {
        zframe_destroy (&self->routing_id);
        self->routing_id = zframe_recv (input);
        if (!self->routing_id || !zsock_rcvmore (input)) {
            zsys_warning ("actor_message: no routing ID");
            return -1;          //  Interrupted or malformed
        }
    }
    zmq_msg_t frame;
    zmq_msg_init (&frame);
    int size = zmq_msg_recv (&frame, zsock_resolve (input), 0);
    if (size == -1) {
        zsys_warning ("actor_message: interrupted");
        goto malformed;         //  Interrupted
    }
    //  Get and check protocol signature
    self->needle = (byte *) zmq_msg_data (&frame);
    self->ceiling = self->needle + zmq_msg_size (&frame);

    uint16_t signature;
    GET_NUMBER2 (signature);
    if (signature != (0xAAA0 | 7)) {
        zsys_warning ("actor_message: invalid signature");
        //  TODO: discard invalid messages and loop, and return
        //  -1 only on interrupt
        goto malformed;         //  Interrupted
    }
    //  Get message id and parse per message type
    GET_NUMBER1 (self->id);

    switch (self->id) {
        case ACTOR_MESSAGE_INITIALIZE:
            GET_STRING (self->mrb_class);
            {
                size_t chunk_size;
                GET_NUMBER4 (chunk_size);
                if (self->needle + chunk_size > (self->ceiling)) {
                    zsys_warning ("actor_message: args is missing data");
                    goto malformed;
                }
                zchunk_destroy (&self->args);
                self->args = zchunk_new (self->needle, chunk_size);
                self->needle += chunk_size;
            }
            break;

        case ACTOR_MESSAGE_INITIALIZE_OK:
            GET_NUMBER8 (self->object_id);
            break;

        case ACTOR_MESSAGE_SEND_MESSAGE:
            GET_NUMBER8 (self->object_id);
            GET_STRING (self->method);
            {
                size_t chunk_size;
                GET_NUMBER4 (chunk_size);
                if (self->needle + chunk_size > (self->ceiling)) {
                    zsys_warning ("actor_message: args is missing data");
                    goto malformed;
                }
                zchunk_destroy (&self->args);
                self->args = zchunk_new (self->needle, chunk_size);
                self->needle += chunk_size;
            }
            break;

        case ACTOR_MESSAGE_SEND_OK:
            {
                size_t chunk_size;
                GET_NUMBER4 (chunk_size);
                if (self->needle + chunk_size > (self->ceiling)) {
                    zsys_warning ("actor_message: result is missing data");
                    goto malformed;
                }
                zchunk_destroy (&self->result);
                self->result = zchunk_new (self->needle, chunk_size);
                self->needle += chunk_size;
            }
            break;

        case ACTOR_MESSAGE_ASYNC_SEND_MESSAGE:
            if (self->needle + ZUUID_LEN > (self->ceiling)) {
                zsys_warning ("actor_message: uuid is invalid");
                goto malformed;
            }
            zuuid_destroy (&self->uuid);
            self->uuid = zuuid_new_from (self->needle);
            self->needle += ZUUID_LEN;
            GET_NUMBER8 (self->object_id);
            GET_STRING (self->method);
            {
                size_t chunk_size;
                GET_NUMBER4 (chunk_size);
                if (self->needle + chunk_size > (self->ceiling)) {
                    zsys_warning ("actor_message: args is missing data");
                    goto malformed;
                }
                zchunk_destroy (&self->args);
                self->args = zchunk_new (self->needle, chunk_size);
                self->needle += chunk_size;
            }
            break;

        case ACTOR_MESSAGE_ASYNC_SEND_OK:
            if (self->needle + ZUUID_LEN > (self->ceiling)) {
                zsys_warning ("actor_message: uuid is invalid");
                goto malformed;
            }
            zuuid_destroy (&self->uuid);
            self->uuid = zuuid_new_from (self->needle);
            self->needle += ZUUID_LEN;
            {
                size_t chunk_size;
                GET_NUMBER4 (chunk_size);
                if (self->needle + chunk_size > (self->ceiling)) {
                    zsys_warning ("actor_message: result is missing data");
                    goto malformed;
                }
                zchunk_destroy (&self->result);
                self->result = zchunk_new (self->needle, chunk_size);
                self->needle += chunk_size;
            }
            break;

        case ACTOR_MESSAGE_ASYNC_ERROR:
            if (self->needle + ZUUID_LEN > (self->ceiling)) {
                zsys_warning ("actor_message: uuid is invalid");
                goto malformed;
            }
            zuuid_destroy (&self->uuid);
            self->uuid = zuuid_new_from (self->needle);
            self->needle += ZUUID_LEN;
            GET_STRING (self->mrb_class);
            GET_STRING (self->error);
            break;

        case ACTOR_MESSAGE_ERROR:
            GET_STRING (self->mrb_class);
            GET_STRING (self->error);
            break;

        default:
            zsys_warning ("actor_message: bad message ID");
            goto malformed;
    }
    //  Successful return
    zmq_msg_close (&frame);
    return 0;

    //  Error returns
    malformed:
        zsys_warning ("actor_message: actor_message malformed message, fail");
        zmq_msg_close (&frame);
        return -1;              //  Invalid message
}


//  --------------------------------------------------------------------------
//  Send the actor_message to the socket. Does not destroy it. Returns 0 if
//  OK, else -1.

int
actor_message_send (actor_message_t *self, zsock_t *output)
{
    assert (self);
    assert (output);

    if (zsock_type (output) == ZMQ_ROUTER)
        zframe_send (&self->routing_id, output, ZFRAME_MORE + ZFRAME_REUSE);

    size_t frame_size = 2 + 1;          //  Signature and message ID
    switch (self->id) {
        case ACTOR_MESSAGE_INITIALIZE:
            frame_size += 1 + strlen (self->mrb_class);
            frame_size += 4;            //  Size is 4 octets
            if (self->args)
                frame_size += zchunk_size (self->args);
            break;
        case ACTOR_MESSAGE_INITIALIZE_OK:
            frame_size += 8;            //  object_id
            break;
        case ACTOR_MESSAGE_SEND_MESSAGE:
            frame_size += 8;            //  object_id
            frame_size += 1 + strlen (self->method);
            frame_size += 4;            //  Size is 4 octets
            if (self->args)
                frame_size += zchunk_size (self->args);
            break;
        case ACTOR_MESSAGE_SEND_OK:
            frame_size += 4;            //  Size is 4 octets
            if (self->result)
                frame_size += zchunk_size (self->result);
            break;
        case ACTOR_MESSAGE_ASYNC_SEND_MESSAGE:
            frame_size += ZUUID_LEN;    //  uuid
            frame_size += 8;            //  object_id
            frame_size += 1 + strlen (self->method);
            frame_size += 4;            //  Size is 4 octets
            if (self->args)
                frame_size += zchunk_size (self->args);
            break;
        case ACTOR_MESSAGE_ASYNC_SEND_OK:
            frame_size += ZUUID_LEN;    //  uuid
            frame_size += 4;            //  Size is 4 octets
            if (self->result)
                frame_size += zchunk_size (self->result);
            break;
        case ACTOR_MESSAGE_ASYNC_ERROR:
            frame_size += ZUUID_LEN;    //  uuid
            frame_size += 1 + strlen (self->mrb_class);
            frame_size += 1 + strlen (self->error);
            break;
        case ACTOR_MESSAGE_ERROR:
            frame_size += 1 + strlen (self->mrb_class);
            frame_size += 1 + strlen (self->error);
            break;
    }
    //  Now serialize message into the frame
    zmq_msg_t frame;
    zmq_msg_init_size (&frame, frame_size);
    self->needle = (byte *) zmq_msg_data (&frame);
    PUT_NUMBER2 (0xAAA0 | 7);
    PUT_NUMBER1 (self->id);
    size_t nbr_frames = 1;              //  Total number of frames to send

    switch (self->id) {
        case ACTOR_MESSAGE_INITIALIZE:
            PUT_STRING (self->mrb_class);
            if (self->args) {
                PUT_NUMBER4 (zchunk_size (self->args));
                memcpy (self->needle,
                        zchunk_data (self->args),
                        zchunk_size (self->args));
                self->needle += zchunk_size (self->args);
            }
            else
                PUT_NUMBER4 (0);    //  Empty chunk
            break;

        case ACTOR_MESSAGE_INITIALIZE_OK:
            PUT_NUMBER8 (self->object_id);
            break;

        case ACTOR_MESSAGE_SEND_MESSAGE:
            PUT_NUMBER8 (self->object_id);
            PUT_STRING (self->method);
            if (self->args) {
                PUT_NUMBER4 (zchunk_size (self->args));
                memcpy (self->needle,
                        zchunk_data (self->args),
                        zchunk_size (self->args));
                self->needle += zchunk_size (self->args);
            }
            else
                PUT_NUMBER4 (0);    //  Empty chunk
            break;

        case ACTOR_MESSAGE_SEND_OK:
            if (self->result) {
                PUT_NUMBER4 (zchunk_size (self->result));
                memcpy (self->needle,
                        zchunk_data (self->result),
                        zchunk_size (self->result));
                self->needle += zchunk_size (self->result);
            }
            else
                PUT_NUMBER4 (0);    //  Empty chunk
            break;

        case ACTOR_MESSAGE_ASYNC_SEND_MESSAGE:
            if (self->uuid)
                zuuid_export (self->uuid, self->needle);
            else
                memset (self->needle, 0, ZUUID_LEN);
            self->needle += ZUUID_LEN;
            PUT_NUMBER8 (self->object_id);
            PUT_STRING (self->method);
            if (self->args) {
                PUT_NUMBER4 (zchunk_size (self->args));
                memcpy (self->needle,
                        zchunk_data (self->args),
                        zchunk_size (self->args));
                self->needle += zchunk_size (self->args);
            }
            else
                PUT_NUMBER4 (0);    //  Empty chunk
            break;

        case ACTOR_MESSAGE_ASYNC_SEND_OK:
            if (self->uuid)
                zuuid_export (self->uuid, self->needle);
            else
                memset (self->needle, 0, ZUUID_LEN);
            self->needle += ZUUID_LEN;
            if (self->result) {
                PUT_NUMBER4 (zchunk_size (self->result));
                memcpy (self->needle,
                        zchunk_data (self->result),
                        zchunk_size (self->result));
                self->needle += zchunk_size (self->result);
            }
            else
                PUT_NUMBER4 (0);    //  Empty chunk
            break;

        case ACTOR_MESSAGE_ASYNC_ERROR:
            if (self->uuid)
                zuuid_export (self->uuid, self->needle);
            else
                memset (self->needle, 0, ZUUID_LEN);
            self->needle += ZUUID_LEN;
            PUT_STRING (self->mrb_class);
            PUT_STRING (self->error);
            break;

        case ACTOR_MESSAGE_ERROR:
            PUT_STRING (self->mrb_class);
            PUT_STRING (self->error);
            break;

    }
    //  Now send the data frame
    zmq_msg_send (&frame, zsock_resolve (output), --nbr_frames? ZMQ_SNDMORE: 0);

    return 0;
}


//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
actor_message_print (actor_message_t *self)
{
    assert (self);
    switch (self->id) {
        case ACTOR_MESSAGE_INITIALIZE:
            zsys_debug ("ACTOR_MESSAGE_INITIALIZE:");
            zsys_debug ("    mrb_class='%s'", self->mrb_class);
            zsys_debug ("    args=[ ... ]");
            break;

        case ACTOR_MESSAGE_INITIALIZE_OK:
            zsys_debug ("ACTOR_MESSAGE_INITIALIZE_OK:");
            zsys_debug ("    object_id=%ld", (long) self->object_id);
            break;

        case ACTOR_MESSAGE_SEND_MESSAGE:
            zsys_debug ("ACTOR_MESSAGE_SEND_MESSAGE:");
            zsys_debug ("    object_id=%ld", (long) self->object_id);
            zsys_debug ("    method='%s'", self->method);
            zsys_debug ("    args=[ ... ]");
            break;

        case ACTOR_MESSAGE_SEND_OK:
            zsys_debug ("ACTOR_MESSAGE_SEND_OK:");
            zsys_debug ("    result=[ ... ]");
            break;

        case ACTOR_MESSAGE_ASYNC_SEND_MESSAGE:
            zsys_debug ("ACTOR_MESSAGE_ASYNC_SEND_MESSAGE:");
            zsys_debug ("    uuid=");
            if (self->uuid)
                zsys_debug ("        %s", zuuid_str_canonical (self->uuid));
            else
                zsys_debug ("        (NULL)");
            zsys_debug ("    object_id=%ld", (long) self->object_id);
            zsys_debug ("    method='%s'", self->method);
            zsys_debug ("    args=[ ... ]");
            break;

        case ACTOR_MESSAGE_ASYNC_SEND_OK:
            zsys_debug ("ACTOR_MESSAGE_ASYNC_SEND_OK:");
            zsys_debug ("    uuid=");
            if (self->uuid)
                zsys_debug ("        %s", zuuid_str_canonical (self->uuid));
            else
                zsys_debug ("        (NULL)");
            zsys_debug ("    result=[ ... ]");
            break;

        case ACTOR_MESSAGE_ASYNC_ERROR:
            zsys_debug ("ACTOR_MESSAGE_ASYNC_ERROR:");
            zsys_debug ("    uuid=");
            if (self->uuid)
                zsys_debug ("        %s", zuuid_str_canonical (self->uuid));
            else
                zsys_debug ("        (NULL)");
            zsys_debug ("    mrb_class='%s'", self->mrb_class);
            zsys_debug ("    error='%s'", self->error);
            break;

        case ACTOR_MESSAGE_ERROR:
            zsys_debug ("ACTOR_MESSAGE_ERROR:");
            zsys_debug ("    mrb_class='%s'", self->mrb_class);
            zsys_debug ("    error='%s'", self->error);
            break;

    }
}


//  --------------------------------------------------------------------------
//  Get/set the message routing_id

zframe_t *
actor_message_routing_id (actor_message_t *self)
{
    assert (self);
    return self->routing_id;
}

void
actor_message_set_routing_id (actor_message_t *self, zframe_t *routing_id)
{
    if (self->routing_id)
        zframe_destroy (&self->routing_id);
    self->routing_id = zframe_dup (routing_id);
}


//  --------------------------------------------------------------------------
//  Get/set the actor_message id

int
actor_message_id (actor_message_t *self)
{
    assert (self);
    return self->id;
}

void
actor_message_set_id (actor_message_t *self, int id)
{
    self->id = id;
}

//  --------------------------------------------------------------------------
//  Return a printable command string

const char *
actor_message_command (actor_message_t *self)
{
    assert (self);
    switch (self->id) {
        case ACTOR_MESSAGE_INITIALIZE:
            return ("INITIALIZE");
            break;
        case ACTOR_MESSAGE_INITIALIZE_OK:
            return ("INITIALIZE_OK");
            break;
        case ACTOR_MESSAGE_SEND_MESSAGE:
            return ("SEND_MESSAGE");
            break;
        case ACTOR_MESSAGE_SEND_OK:
            return ("SEND_OK");
            break;
        case ACTOR_MESSAGE_ASYNC_SEND_MESSAGE:
            return ("ASYNC_SEND_MESSAGE");
            break;
        case ACTOR_MESSAGE_ASYNC_SEND_OK:
            return ("ASYNC_SEND_OK");
            break;
        case ACTOR_MESSAGE_ASYNC_ERROR:
            return ("ASYNC_ERROR");
            break;
        case ACTOR_MESSAGE_ERROR:
            return ("ERROR");
            break;
    }
    return "?";
}

//  --------------------------------------------------------------------------
//  Get/set the mrb_class field

const char *
actor_message_mrb_class (actor_message_t *self)
{
    assert (self);
    return self->mrb_class;
}

void
actor_message_set_mrb_class (actor_message_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->mrb_class)
        return;
    strncpy (self->mrb_class, value, 255);
    self->mrb_class [255] = 0;
}


//  --------------------------------------------------------------------------
//  Get the args field without transferring ownership

zchunk_t *
actor_message_args (actor_message_t *self)
{
    assert (self);
    return self->args;
}

//  Get the args field and transfer ownership to caller

zchunk_t *
actor_message_get_args (actor_message_t *self)
{
    zchunk_t *args = self->args;
    self->args = NULL;
    return args;
}

//  Set the args field, transferring ownership from caller

void
actor_message_set_args (actor_message_t *self, zchunk_t **chunk_p)
{
    assert (self);
    assert (chunk_p);
    zchunk_destroy (&self->args);
    self->args = *chunk_p;
    *chunk_p = NULL;
}


//  --------------------------------------------------------------------------
//  Get/set the object_id field

uint64_t
actor_message_object_id (actor_message_t *self)
{
    assert (self);
    return self->object_id;
}

void
actor_message_set_object_id (actor_message_t *self, uint64_t object_id)
{
    assert (self);
    self->object_id = object_id;
}


//  --------------------------------------------------------------------------
//  Get/set the method field

const char *
actor_message_method (actor_message_t *self)
{
    assert (self);
    return self->method;
}

void
actor_message_set_method (actor_message_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->method)
        return;
    strncpy (self->method, value, 255);
    self->method [255] = 0;
}


//  --------------------------------------------------------------------------
//  Get the result field without transferring ownership

zchunk_t *
actor_message_result (actor_message_t *self)
{
    assert (self);
    return self->result;
}

//  Get the result field and transfer ownership to caller

zchunk_t *
actor_message_get_result (actor_message_t *self)
{
    zchunk_t *result = self->result;
    self->result = NULL;
    return result;
}

//  Set the result field, transferring ownership from caller

void
actor_message_set_result (actor_message_t *self, zchunk_t **chunk_p)
{
    assert (self);
    assert (chunk_p);
    zchunk_destroy (&self->result);
    self->result = *chunk_p;
    *chunk_p = NULL;
}


//  --------------------------------------------------------------------------
//  Get/set the uuid field
zuuid_t *
actor_message_uuid (actor_message_t *self)
{
    assert (self);
    return self->uuid;
}

void
actor_message_set_uuid (actor_message_t *self, zuuid_t *uuid)
{
    assert (self);
    zuuid_destroy (&self->uuid);
    self->uuid = zuuid_dup (uuid);
}

//  Get the uuid field and transfer ownership to caller

zuuid_t *
actor_message_get_uuid (actor_message_t *self)
{
    zuuid_t *uuid = self->uuid;
    self->uuid = NULL;
    return uuid;
}


//  --------------------------------------------------------------------------
//  Get/set the error field

const char *
actor_message_error (actor_message_t *self)
{
    assert (self);
    return self->error;
}

void
actor_message_set_error (actor_message_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->error)
        return;
    strncpy (self->error, value, 255);
    self->error [255] = 0;
}



//  --------------------------------------------------------------------------
//  Selftest

int
actor_message_test (bool verbose)
{
    printf (" * actor_message:");

    if (verbose)
        printf ("\n");

    //  @selftest
    //  Simple create/destroy test
    actor_message_t *self = actor_message_new ();
    assert (self);
    actor_message_destroy (&self);
    //  Create pair of sockets we can send through
    //  We must bind before connect if we wish to remain compatible with ZeroMQ < v4
    zsock_t *output = zsock_new (ZMQ_DEALER);
    assert (output);
    int rc = zsock_bind (output, "inproc://selftest-actor_message");
    assert (rc == 0);

    zsock_t *input = zsock_new (ZMQ_ROUTER);
    assert (input);
    rc = zsock_connect (input, "inproc://selftest-actor_message");
    assert (rc == 0);


    //  Encode/send/decode and verify each message type
    int instance;
    self = actor_message_new ();
    actor_message_set_id (self, ACTOR_MESSAGE_INITIALIZE);

    actor_message_set_mrb_class (self, "Life is short but Now lasts for ever");
    zchunk_t *initialize_args = zchunk_new ("Captcha Diem", 12);
    actor_message_set_args (self, &initialize_args);
    //  Send twice
    actor_message_send (self, output);
    actor_message_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        actor_message_recv (self, input);
        assert (actor_message_routing_id (self));
        assert (streq (actor_message_mrb_class (self), "Life is short but Now lasts for ever"));
        assert (memcmp (zchunk_data (actor_message_args (self)), "Captcha Diem", 12) == 0);
        if (instance == 1)
            zchunk_destroy (&initialize_args);
    }
    actor_message_set_id (self, ACTOR_MESSAGE_INITIALIZE_OK);

    actor_message_set_object_id (self, 123);
    //  Send twice
    actor_message_send (self, output);
    actor_message_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        actor_message_recv (self, input);
        assert (actor_message_routing_id (self));
        assert (actor_message_object_id (self) == 123);
    }
    actor_message_set_id (self, ACTOR_MESSAGE_SEND_MESSAGE);

    actor_message_set_object_id (self, 123);
    actor_message_set_method (self, "Life is short but Now lasts for ever");
    zchunk_t *send_message_args = zchunk_new ("Captcha Diem", 12);
    actor_message_set_args (self, &send_message_args);
    //  Send twice
    actor_message_send (self, output);
    actor_message_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        actor_message_recv (self, input);
        assert (actor_message_routing_id (self));
        assert (actor_message_object_id (self) == 123);
        assert (streq (actor_message_method (self), "Life is short but Now lasts for ever"));
        assert (memcmp (zchunk_data (actor_message_args (self)), "Captcha Diem", 12) == 0);
        if (instance == 1)
            zchunk_destroy (&send_message_args);
    }
    actor_message_set_id (self, ACTOR_MESSAGE_SEND_OK);

    zchunk_t *send_ok_result = zchunk_new ("Captcha Diem", 12);
    actor_message_set_result (self, &send_ok_result);
    //  Send twice
    actor_message_send (self, output);
    actor_message_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        actor_message_recv (self, input);
        assert (actor_message_routing_id (self));
        assert (memcmp (zchunk_data (actor_message_result (self)), "Captcha Diem", 12) == 0);
        if (instance == 1)
            zchunk_destroy (&send_ok_result);
    }
    actor_message_set_id (self, ACTOR_MESSAGE_ASYNC_SEND_MESSAGE);

    zuuid_t *async_send_message_uuid = zuuid_new ();
    actor_message_set_uuid (self, async_send_message_uuid);
    actor_message_set_object_id (self, 123);
    actor_message_set_method (self, "Life is short but Now lasts for ever");
    zchunk_t *async_send_message_args = zchunk_new ("Captcha Diem", 12);
    actor_message_set_args (self, &async_send_message_args);
    //  Send twice
    actor_message_send (self, output);
    actor_message_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        actor_message_recv (self, input);
        assert (actor_message_routing_id (self));
        assert (zuuid_eq (async_send_message_uuid, zuuid_data (actor_message_uuid (self))));
        if (instance == 1)
            zuuid_destroy (&async_send_message_uuid);
        assert (actor_message_object_id (self) == 123);
        assert (streq (actor_message_method (self), "Life is short but Now lasts for ever"));
        assert (memcmp (zchunk_data (actor_message_args (self)), "Captcha Diem", 12) == 0);
        if (instance == 1)
            zchunk_destroy (&async_send_message_args);
    }
    actor_message_set_id (self, ACTOR_MESSAGE_ASYNC_SEND_OK);

    zuuid_t *async_send_ok_uuid = zuuid_new ();
    actor_message_set_uuid (self, async_send_ok_uuid);
    zchunk_t *async_send_ok_result = zchunk_new ("Captcha Diem", 12);
    actor_message_set_result (self, &async_send_ok_result);
    //  Send twice
    actor_message_send (self, output);
    actor_message_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        actor_message_recv (self, input);
        assert (actor_message_routing_id (self));
        assert (zuuid_eq (async_send_ok_uuid, zuuid_data (actor_message_uuid (self))));
        if (instance == 1)
            zuuid_destroy (&async_send_ok_uuid);
        assert (memcmp (zchunk_data (actor_message_result (self)), "Captcha Diem", 12) == 0);
        if (instance == 1)
            zchunk_destroy (&async_send_ok_result);
    }
    actor_message_set_id (self, ACTOR_MESSAGE_ASYNC_ERROR);

    zuuid_t *async_error_uuid = zuuid_new ();
    actor_message_set_uuid (self, async_error_uuid);
    actor_message_set_mrb_class (self, "Life is short but Now lasts for ever");
    actor_message_set_error (self, "Life is short but Now lasts for ever");
    //  Send twice
    actor_message_send (self, output);
    actor_message_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        actor_message_recv (self, input);
        assert (actor_message_routing_id (self));
        assert (zuuid_eq (async_error_uuid, zuuid_data (actor_message_uuid (self))));
        if (instance == 1)
            zuuid_destroy (&async_error_uuid);
        assert (streq (actor_message_mrb_class (self), "Life is short but Now lasts for ever"));
        assert (streq (actor_message_error (self), "Life is short but Now lasts for ever"));
    }
    actor_message_set_id (self, ACTOR_MESSAGE_ERROR);

    actor_message_set_mrb_class (self, "Life is short but Now lasts for ever");
    actor_message_set_error (self, "Life is short but Now lasts for ever");
    //  Send twice
    actor_message_send (self, output);
    actor_message_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        actor_message_recv (self, input);
        assert (actor_message_routing_id (self));
        assert (streq (actor_message_mrb_class (self), "Life is short but Now lasts for ever"));
        assert (streq (actor_message_error (self), "Life is short but Now lasts for ever"));
    }

    actor_message_destroy (&self);
    zsock_destroy (&input);
    zsock_destroy (&output);
    //  @end

    printf ("OK\n");
    return 0;
}

//  zproto_codec_mruby.gsl
//  Generates a codec for a protocol specification.


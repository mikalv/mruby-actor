/*  =========================================================================
    actor_discovery - mruby actor discovery

    Codec class for actor_discovery.

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: mruby_actor_discovery.xml, or
     * The code generation script that built this file: zproto_codec_c
    ************************************************************************
    =========================================================================
*/

/*
@header
    actor_discovery - mruby actor discovery
@discuss
@end
*/

#include "./actor_discovery.h"

//  Structure of our class

struct _actor_discovery_t {
    zframe_t *routing_id;               //  Routing_id from ROUTER, if any
    int id;                             //  actor_discovery message ID
    byte *needle;                       //  Read/write pointer for serialization
    byte *ceiling;                      //  Valid upper limit for read pointer
    char mrb_class [256];               //  mrb_class
    uint64_t object_id;                 //  object_id
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
        zsys_warning ("actor_discovery: GET_OCTETS failed"); \
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
        zsys_warning ("actor_discovery: GET_NUMBER1 failed"); \
        goto malformed; \
    } \
    (host) = *(byte *) self->needle; \
    self->needle++; \
}

//  Get a 2-byte number from the frame
#define GET_NUMBER2(host) { \
    if (self->needle + 2 > self->ceiling) { \
        zsys_warning ("actor_discovery: GET_NUMBER2 failed"); \
        goto malformed; \
    } \
    (host) = ((uint16_t) (self->needle [0]) << 8) \
           +  (uint16_t) (self->needle [1]); \
    self->needle += 2; \
}

//  Get a 4-byte number from the frame
#define GET_NUMBER4(host) { \
    if (self->needle + 4 > self->ceiling) { \
        zsys_warning ("actor_discovery: GET_NUMBER4 failed"); \
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
        zsys_warning ("actor_discovery: GET_NUMBER8 failed"); \
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
        zsys_warning ("actor_discovery: GET_STRING failed"); \
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
        zsys_warning ("actor_discovery: GET_LONGSTR failed"); \
        goto malformed; \
    } \
    free ((host)); \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}


//  --------------------------------------------------------------------------
//  Create a new actor_discovery

actor_discovery_t *
actor_discovery_new (void)
{
    actor_discovery_t *self = (actor_discovery_t *) zmalloc (sizeof (actor_discovery_t));
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the actor_discovery

void
actor_discovery_destroy (actor_discovery_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        actor_discovery_t *self = *self_p;

        //  Free class properties
        zframe_destroy (&self->routing_id);

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Deserialize a actor_discovery from the specified message, popping
//  as many frames as needed. Returns 0 if OK, -1 if there was an error.
int
actor_discovery_recv (actor_discovery_t *self, zmsg_t *input)
{
    assert (input);
    zframe_t *frame = zmsg_pop (input);
    if (!frame) {
        zsys_warning ("actor_discovery: missing frames in message");
        goto malformed;         //  Interrupted
    }
    //  Get and check protocol signature
    self->needle = zframe_data (frame);
    self->ceiling = self->needle + zframe_size (frame);

    uint16_t signature;
    GET_NUMBER2 (signature);
    if (signature != (0xAAA0 | 8)) {
        zsys_warning ("actor_discovery: invalid signature");
        goto malformed;         //  Interrupted
    }
    //  Get message id and parse per message type
    GET_NUMBER1 (self->id);

    switch (self->id) {
        case ACTOR_DISCOVERY_OBJECT_NEW:
            {
                byte version;
                GET_NUMBER1 (version);
                if (version != 1) {
                    zsys_warning ("actor_discovery: version is invalid");
                    goto malformed;
                }
            }
            GET_STRING (self->mrb_class);
            GET_NUMBER8 (self->object_id);
            break;

        default:
            zsys_warning ("actor_discovery: bad message ID");
            goto malformed;
    }
    zframe_destroy (&frame);
    //  Successful return
    return 0;

    //  Error returns
    malformed:
        zframe_destroy (&frame);
        zsys_warning ("actor_discovery: actor_discovery malformed message, fail");
        return -1;              //  Invalid message
}


//  --------------------------------------------------------------------------
//  Serialize and append the actor_discovery to the specified message
int
actor_discovery_send (actor_discovery_t *self, zmsg_t *output)
{
    assert (self);
    assert (output);
    size_t frame_size = 2 + 1;          //  Signature and message ID
    switch (self->id) {
        case ACTOR_DISCOVERY_OBJECT_NEW:
            frame_size += 1;            //  version
            frame_size += 1 + strlen (self->mrb_class);
            frame_size += 8;            //  object_id
            break;
    }
    //  Now serialize message into the frame
    zframe_t *frame = zframe_new (NULL, frame_size);
    self->needle = zframe_data (frame);
    PUT_NUMBER2 (0xAAA0 | 8);
    PUT_NUMBER1 (self->id);

    switch (self->id) {
        case ACTOR_DISCOVERY_OBJECT_NEW:
            PUT_NUMBER1 (1);
            PUT_STRING (self->mrb_class);
            PUT_NUMBER8 (self->object_id);
            break;

    }
    //  Now store the frame data
    zmsg_append (output, &frame);

    return 0;
}


//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
actor_discovery_print (actor_discovery_t *self)
{
    assert (self);
    switch (self->id) {
        case ACTOR_DISCOVERY_OBJECT_NEW:
            zsys_debug ("ACTOR_DISCOVERY_OBJECT_NEW:");
            zsys_debug ("    version=1");
            zsys_debug ("    mrb_class='%s'", self->mrb_class);
            zsys_debug ("    object_id=%ld", (long) self->object_id);
            break;

    }
}


//  --------------------------------------------------------------------------
//  Get/set the message routing_id

zframe_t *
actor_discovery_routing_id (actor_discovery_t *self)
{
    assert (self);
    return self->routing_id;
}

void
actor_discovery_set_routing_id (actor_discovery_t *self, zframe_t *routing_id)
{
    if (self->routing_id)
        zframe_destroy (&self->routing_id);
    self->routing_id = zframe_dup (routing_id);
}


//  --------------------------------------------------------------------------
//  Get/set the actor_discovery id

int
actor_discovery_id (actor_discovery_t *self)
{
    assert (self);
    return self->id;
}

void
actor_discovery_set_id (actor_discovery_t *self, int id)
{
    self->id = id;
}

//  --------------------------------------------------------------------------
//  Return a printable command string

const char *
actor_discovery_command (actor_discovery_t *self)
{
    assert (self);
    switch (self->id) {
        case ACTOR_DISCOVERY_OBJECT_NEW:
            return ("OBJECT_NEW");
            break;
    }
    return "?";
}

//  --------------------------------------------------------------------------
//  Get/set the mrb_class field

const char *
actor_discovery_mrb_class (actor_discovery_t *self)
{
    assert (self);
    return self->mrb_class;
}

void
actor_discovery_set_mrb_class (actor_discovery_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->mrb_class)
        return;
    strncpy (self->mrb_class, value, 255);
    self->mrb_class [255] = 0;
}


//  --------------------------------------------------------------------------
//  Get/set the object_id field

uint64_t
actor_discovery_object_id (actor_discovery_t *self)
{
    assert (self);
    return self->object_id;
}

void
actor_discovery_set_object_id (actor_discovery_t *self, uint64_t object_id)
{
    assert (self);
    self->object_id = object_id;
}



//  --------------------------------------------------------------------------
//  Selftest

int
actor_discovery_test (bool verbose)
{
    printf (" * actor_discovery:");

    if (verbose)
        printf ("\n");

    //  @selftest
    //  Simple create/destroy test
    actor_discovery_t *self = actor_discovery_new ();
    assert (self);
    actor_discovery_destroy (&self);
    zmsg_t *output = zmsg_new ();
    assert (output);

    zmsg_t *input = zmsg_new ();
    assert (input);


    //  Encode/send/decode and verify each message type
    int instance;
    self = actor_discovery_new ();
    actor_discovery_set_id (self, ACTOR_DISCOVERY_OBJECT_NEW);

    actor_discovery_set_mrb_class (self, "Life is short but Now lasts for ever");
    actor_discovery_set_object_id (self, 123);
    zmsg_destroy (&output);
    output = zmsg_new ();
    assert (output);
    //  Send twice
    actor_discovery_send (self, output);
    actor_discovery_send (self, output);

    zmsg_destroy (&input);
    input = zmsg_dup (output);
    assert (input);
    for (instance = 0; instance < 2; instance++) {
        actor_discovery_recv (self, input);
        assert (actor_discovery_routing_id (self) == NULL);
        assert (streq (actor_discovery_mrb_class (self), "Life is short but Now lasts for ever"));
        assert (actor_discovery_object_id (self) == 123);
    }

    actor_discovery_destroy (&self);
    zmsg_destroy (&input);
    zmsg_destroy (&output);
    //  @end

    printf ("OK\n");
    return 0;
}

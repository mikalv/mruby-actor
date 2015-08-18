/*  =========================================================================
    actor_message - mruby actor messages

    Codec header for actor_message.

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

#ifndef ACTOR_MESSAGE_H_INCLUDED
#define ACTOR_MESSAGE_H_INCLUDED

/*  These are the actor_message messages:

    INITIALIZE - 
        mrb_class           string      
        args                chunk       

    INITIALIZE_OK - 
        object_id           number 8    

    SEND_MESSAGE - 
        object_id           number 8    
        method              string      
        args                chunk       

    SEND_OK - 
        result              chunk       

    ASYNC_SEND_MESSAGE - 
        uuid                uuid        
        object_id           number 8    
        method              string      
        args                chunk       

    ASYNC_SEND_OK - 
        uuid                uuid        
        result              chunk       

    ASYNC_ERROR - 
        uuid                uuid        
        mrb_class           string      
        error               string      

    ERROR - 
        mrb_class           string      
        error               string      
*/


#define ACTOR_MESSAGE_INITIALIZE            1
#define ACTOR_MESSAGE_INITIALIZE_OK         2
#define ACTOR_MESSAGE_SEND_MESSAGE          3
#define ACTOR_MESSAGE_SEND_OK               4
#define ACTOR_MESSAGE_ASYNC_SEND_MESSAGE    5
#define ACTOR_MESSAGE_ASYNC_SEND_OK         6
#define ACTOR_MESSAGE_ASYNC_ERROR           7
#define ACTOR_MESSAGE_ERROR                 8

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
#ifndef ACTOR_MESSAGE_T_DEFINED
typedef struct _actor_message_t actor_message_t;
#define ACTOR_MESSAGE_T_DEFINED
#endif

//  @interface
//  Create a new empty actor_message
actor_message_t *
    actor_message_new (void);

//  Destroy a actor_message instance
void
    actor_message_destroy (actor_message_t **self_p);

//  Receive a actor_message from the socket. Returns 0 if OK, -1 if
//  there was an error. Blocks if there is no message waiting.
int
    actor_message_recv (actor_message_t *self, zsock_t *input);

//  Send the actor_message to the output socket, does not destroy it
int
    actor_message_send (actor_message_t *self, zsock_t *output);

//  Print contents of message to stdout
void
    actor_message_print (actor_message_t *self);

//  Get/set the message routing id
zframe_t *
    actor_message_routing_id (actor_message_t *self);
void
    actor_message_set_routing_id (actor_message_t *self, zframe_t *routing_id);

//  Get the actor_message id and printable command
int
    actor_message_id (actor_message_t *self);
void
    actor_message_set_id (actor_message_t *self, int id);
const char *
    actor_message_command (actor_message_t *self);

//  Get/set the mrb_class field
const char *
    actor_message_mrb_class (actor_message_t *self);
void
    actor_message_set_mrb_class (actor_message_t *self, const char *value);

//  Get a copy of the args field
zchunk_t *
    actor_message_args (actor_message_t *self);
//  Get the args field and transfer ownership to caller
zchunk_t *
    actor_message_get_args (actor_message_t *self);
//  Set the args field, transferring ownership from caller
void
    actor_message_set_args (actor_message_t *self, zchunk_t **chunk_p);

//  Get/set the object_id field
uint64_t
    actor_message_object_id (actor_message_t *self);
void
    actor_message_set_object_id (actor_message_t *self, uint64_t object_id);

//  Get/set the method field
const char *
    actor_message_method (actor_message_t *self);
void
    actor_message_set_method (actor_message_t *self, const char *value);

//  Get a copy of the result field
zchunk_t *
    actor_message_result (actor_message_t *self);
//  Get the result field and transfer ownership to caller
zchunk_t *
    actor_message_get_result (actor_message_t *self);
//  Set the result field, transferring ownership from caller
void
    actor_message_set_result (actor_message_t *self, zchunk_t **chunk_p);

//  Get/set the uuid field
zuuid_t *
    actor_message_uuid (actor_message_t *self);
void
    actor_message_set_uuid (actor_message_t *self, zuuid_t *uuid);
//  Get the uuid field and transfer ownership to caller
zuuid_t *
    actor_message_get_uuid (actor_message_t *self);

//  Get/set the error field
const char *
    actor_message_error (actor_message_t *self);
void
    actor_message_set_error (actor_message_t *self, const char *value);

//  Self test of this class
int
    actor_message_test (bool verbose);
//  @end

//  For backwards compatibility with old codecs
#define actor_message_dump  actor_message_print

#ifdef __cplusplus
}
#endif

#endif

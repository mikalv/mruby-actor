/*  =========================================================================
    mrb_actor_msg - mrb actor messages

    Codec header for mrb_actor_msg.

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: mrb_actor_msg.xml, or
     * The code generation script that built this file: zproto_codec_c
    ************************************************************************
    =========================================================================
*/

#ifndef MRB_ACTOR_MSG_H_INCLUDED
#define MRB_ACTOR_MSG_H_INCLUDED

/*  These are the mrb_actor_msg messages:

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


#define MRB_ACTOR_MSG_INITIALIZE            1
#define MRB_ACTOR_MSG_INITIALIZE_OK         2
#define MRB_ACTOR_MSG_SEND_MESSAGE          3
#define MRB_ACTOR_MSG_SEND_OK               4
#define MRB_ACTOR_MSG_ASYNC_SEND_MESSAGE    5
#define MRB_ACTOR_MSG_ASYNC_SEND_OK         6
#define MRB_ACTOR_MSG_ASYNC_ERROR           7
#define MRB_ACTOR_MSG_ERROR                 8

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
#ifndef MRB_ACTOR_MSG_T_DEFINED
typedef struct _mrb_actor_msg_t mrb_actor_msg_t;
#define MRB_ACTOR_MSG_T_DEFINED
#endif

//  @interface
//  Create a new empty mrb_actor_msg
mrb_actor_msg_t *
    mrb_actor_msg_new (void);

//  Destroy a mrb_actor_msg instance
void
    mrb_actor_msg_destroy (mrb_actor_msg_t **self_p);

//  Receive a mrb_actor_msg from the socket. Returns 0 if OK, -1 if
//  there was an error. Blocks if there is no message waiting.
int
    mrb_actor_msg_recv (mrb_actor_msg_t *self, zsock_t *input);

//  Send the mrb_actor_msg to the output socket, does not destroy it
int
    mrb_actor_msg_send (mrb_actor_msg_t *self, zsock_t *output);

//  Print contents of message to stdout
void
    mrb_actor_msg_print (mrb_actor_msg_t *self);

//  Get/set the message routing id
zframe_t *
    mrb_actor_msg_routing_id (mrb_actor_msg_t *self);
void
    mrb_actor_msg_set_routing_id (mrb_actor_msg_t *self, zframe_t *routing_id);

//  Get the mrb_actor_msg id and printable command
int
    mrb_actor_msg_id (mrb_actor_msg_t *self);
void
    mrb_actor_msg_set_id (mrb_actor_msg_t *self, int id);
const char *
    mrb_actor_msg_command (mrb_actor_msg_t *self);

//  Get/set the mrb_class field
const char *
    mrb_actor_msg_mrb_class (mrb_actor_msg_t *self);
void
    mrb_actor_msg_set_mrb_class (mrb_actor_msg_t *self, const char *value);

//  Get a copy of the args field
zchunk_t *
    mrb_actor_msg_args (mrb_actor_msg_t *self);
//  Get the args field and transfer ownership to caller
zchunk_t *
    mrb_actor_msg_get_args (mrb_actor_msg_t *self);
//  Set the args field, transferring ownership from caller
void
    mrb_actor_msg_set_args (mrb_actor_msg_t *self, zchunk_t **chunk_p);

//  Get/set the object_id field
uint64_t
    mrb_actor_msg_object_id (mrb_actor_msg_t *self);
void
    mrb_actor_msg_set_object_id (mrb_actor_msg_t *self, uint64_t object_id);

//  Get/set the method field
const char *
    mrb_actor_msg_method (mrb_actor_msg_t *self);
void
    mrb_actor_msg_set_method (mrb_actor_msg_t *self, const char *value);

//  Get a copy of the result field
zchunk_t *
    mrb_actor_msg_result (mrb_actor_msg_t *self);
//  Get the result field and transfer ownership to caller
zchunk_t *
    mrb_actor_msg_get_result (mrb_actor_msg_t *self);
//  Set the result field, transferring ownership from caller
void
    mrb_actor_msg_set_result (mrb_actor_msg_t *self, zchunk_t **chunk_p);

//  Get/set the uuid field
zuuid_t *
    mrb_actor_msg_uuid (mrb_actor_msg_t *self);
void
    mrb_actor_msg_set_uuid (mrb_actor_msg_t *self, zuuid_t *uuid);
//  Get the uuid field and transfer ownership to caller
zuuid_t *
    mrb_actor_msg_get_uuid (mrb_actor_msg_t *self);

//  Get/set the error field
const char *
    mrb_actor_msg_error (mrb_actor_msg_t *self);
void
    mrb_actor_msg_set_error (mrb_actor_msg_t *self, const char *value);

//  Self test of this class
int
    mrb_actor_msg_test (bool verbose);
//  @end

//  For backwards compatibility with old codecs
#define mrb_actor_msg_dump  mrb_actor_msg_print

#ifdef __cplusplus
}
#endif

#endif

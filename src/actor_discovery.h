/*  =========================================================================
    actor_discovery - mruby actor discovery

    Codec header for actor_discovery.

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

#ifndef ACTOR_DISCOVERY_H_INCLUDED
#define ACTOR_DISCOVERY_H_INCLUDED

/*  These are the actor_discovery messages:

    OBJECT_NEW - 
        version             number 1    
        mrb_class           string      
        object_id           number 8    
*/


#define ACTOR_DISCOVERY_OBJECT_NEW          1

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
#ifndef ACTOR_DISCOVERY_T_DEFINED
typedef struct _actor_discovery_t actor_discovery_t;
#define ACTOR_DISCOVERY_T_DEFINED
#endif

//  @interface
//  Create a new empty actor_discovery
actor_discovery_t *
    actor_discovery_new (void);

//  Destroy a actor_discovery instance
void
    actor_discovery_destroy (actor_discovery_t **self_p);

//  Deserialize a actor_discovery from the specified message, popping
//  as many frames as needed. Returns 0 if OK, -1 if there was an error.
int
    actor_discovery_recv (actor_discovery_t *self, zmsg_t *input);

//  Serialize and append the actor_discovery to the specified message
int
    actor_discovery_send (actor_discovery_t *self, zmsg_t *output);

//  Print contents of message to stdout
void
    actor_discovery_print (actor_discovery_t *self);

//  Get/set the message routing id
zframe_t *
    actor_discovery_routing_id (actor_discovery_t *self);
void
    actor_discovery_set_routing_id (actor_discovery_t *self, zframe_t *routing_id);

//  Get the actor_discovery id and printable command
int
    actor_discovery_id (actor_discovery_t *self);
void
    actor_discovery_set_id (actor_discovery_t *self, int id);
const char *
    actor_discovery_command (actor_discovery_t *self);

//  Get/set the mrb_class field
const char *
    actor_discovery_mrb_class (actor_discovery_t *self);
void
    actor_discovery_set_mrb_class (actor_discovery_t *self, const char *value);

//  Get/set the object_id field
uint64_t
    actor_discovery_object_id (actor_discovery_t *self);
void
    actor_discovery_set_object_id (actor_discovery_t *self, uint64_t object_id);

//  Self test of this class
int
    actor_discovery_test (bool verbose);
//  @end

//  For backwards compatibility with old codecs
#define actor_discovery_dump  actor_discovery_print

#ifdef __cplusplus
}
#endif

#endif


#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include "../hashtable/hashtable.c"

// protobuf-c headers
#include <protobuf-c/protobuf-c.h>
#include <protobuf-c-rpc/protobuf-c-rpc.h>
// Definitions for our protocol (under build/)
#include <protobuf/protocol.pb-c.h>

static char *name;
static hashtable_t* hashtable;


static void example__set (Dkvs__Server_Service    *service,
                  	      const Dkvs__Row  *row,
                              Dkvs__Status_Closure  closure,
                  	      void                       *closure_data)
{
  (void) service;
  if (row == NULL)
    closure (NULL, closure_data);
  else
    {
      ht_set((const char*)row->key.data, (const char*)row->value.data);
      fprintf(stderr, "Key value saved successfully\n");
      Dkvs__Status result = DKVS__STATUS__INIT;
      result.status = 0;
      closure (&result, closure_data);
    }
}


static void example__get (Dkvs__Server_Service    *service,
                  	  const Dkvs__RouterRequest  *router_request,
                          Dkvs__Row_Closure  closure,
                  	  void                       *closure_data)
{
  (void) service;
  if (router_request == NULL)
    closure (NULL, closure_data);
  else
    {
      ProtobufCBinaryData val;
      char* value = ht_get(router_request->key.data);
      
      if(value == NULL){
	fprintf(stderr, "No match found\n");
	val.data = malloc(1);
        val.len  = 1;
        val.data = NULL;
      }
      else{
	fprintf(stderr, "Found\n");
	val.data = malloc(strlen(value)+1);
        val.len  = strlen(value)+1;
        val.data = value;
      }

      Dkvs__Row result = DKVS__ROW__INIT;
      result.key = router_request->key;
      result.value = val;
      closure (&result, closure_data);
    }
}


static void handle_router_config_response (const Dkvs__Status *result, void *closure_data)
{
  if (result == NULL)
    printf ("Error processing request.\n");
  else
    {
      if((int)result->status == 0)
        fprintf (stderr, "Server configured successfully.\n");
    }

  * (protobuf_c_boolean *) closure_data = 1;
}


void configure_server(int action)
{
  // configure server dynamically
  // router client
  ProtobufCService *router_service;
  ProtobufC_RPC_Client *router_client;
  ProtobufC_RPC_AddressType address_type = PROTOBUF_C_RPC_ADDRESS_TCP;
  const char *router_name = "localhost:4747";
  protobuf_c_boolean is_done = 0;

  // creating router client
  router_service = protobuf_c_rpc_client_new (address_type, router_name, &dkvs__router__descriptor, NULL);
  if (router_service == NULL)
    fprintf(stderr, "error creating service");
  router_client = (ProtobufC_RPC_Client *) router_service;

  // connecting to router
  fprintf (stderr, "Connecting to router... ");
  while (!protobuf_c_rpc_client_is_connected (router_client))
    protobuf_c_rpc_dispatch_run (protobuf_c_rpc_dispatch_default ());
  fprintf (stderr, "done.\n"); 
      
  Dkvs__ServerConfig query = DKVS__SERVER_CONFIG__INIT;
  query.address = name;
  query.action = action;

  dkvs__router__configure (router_service, &query, handle_router_config_response, &is_done);

  // loop until operation completes
  while (!is_done)
    protobuf_c_rpc_dispatch_run (protobuf_c_rpc_dispatch_default ());
}


void disconnect_server() 
{
  // disconnect server from router before closing
  configure_server(1);
  fprintf (stderr, "Server disconnected.\n");
  exit(0);
}


static Dkvs__Server_Service the_server_service = DKVS__SERVER__INIT(example__);


int main(int argc, char** argv) {

  // initialize signal handlers for disconnecting
  signal(SIGINT, disconnect_server); 
  //signal(SIGSTOP, disconnect_server);  

  // initializing hashtable
  ht_create(500);

  // initializing server address
  name = argv[1];

  // can only be called after server name is initialized like above
  configure_server(0);
  
  // initializing server
  ProtobufC_RPC_Server *server;
  ProtobufC_RPC_AddressType address_type = PROTOBUF_C_RPC_ADDRESS_TCP;

  server = protobuf_c_rpc_server_new (address_type, name, (ProtobufCService *) &the_server_service, NULL);

  for (;;)
    protobuf_c_rpc_dispatch_run (protobuf_c_rpc_dispatch_default ());
  
  return 0;
}





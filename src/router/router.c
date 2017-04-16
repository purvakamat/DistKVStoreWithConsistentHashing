
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "../hashtable/hashtable.c"

// protobuf-c headers
#include <protobuf-c/protobuf-c.h>
#include <protobuf-c-rpc/protobuf-c-rpc.h>
// Definitions for our protocol (under build/)
#include <protobuf/protocol.pb-c.h>

static int server_count = 0;
static char** server_ports;
static int* server_size;
static char* localhost = "localhost";


static char* get_server_with_memory(){
  int min_size = 99999;
  int j = 0;
  
  for(int i=0; i<server_count; i++) {
    if(server_size[i] < min_size){
	min_size = server_size[i];
 	j = i;
    }
  }

  char* server = NULL;

  if(min_size != 99999){
    server = server_ports[j];
    server_size[j]++;
  }
  
  return server;
}


static void example__get_server (Dkvs__Router_Service    *service,
                  	      const Dkvs__RouterRequest  *router_request,
                              Dkvs__RouterResponse_Closure  closure,
                  	      void                       *closure_data)
{
  (void) service;
  if (router_request == NULL)
    closure (NULL, closure_data);
  else
    {
      fprintf (stderr,"which server has this key? %s\n",(char*)router_request->key.data);
      char* addr = ht_get((char*)router_request->key.data);
      
      if(addr == NULL){
        char* new_port =  get_server_with_memory();
	if(new_port != NULL){
	  addr = malloc(strlen(localhost) + strlen(new_port) + 1);
	  strcpy(addr, localhost);
	  strcat(addr, ":");
	  strcat(addr, new_port);
	  strcat(addr,"\0");
	
	  ht_set((char*)router_request->key.data, addr);
	  fprintf (stderr,"new addr: %s\n",addr);
	}
      }
	
      fprintf (stderr,"addr: %s\n",addr);
      
      Dkvs__RouterResponse result = DKVS__ROUTER_RESPONSE__INIT;
      result.address = addr;
      closure (&result, closure_data);
    }
}


static void example__configure (Dkvs__Router_Service    *service,
                  	      const Dkvs__ServerConfig  *server_config,
                              Dkvs__Status_Closure  closure,
                  	      void                       *closure_data)
{
  (void) service;
  if (server_config == NULL)
    closure (NULL, closure_data);
  else
    {
      fprintf (stderr,"Configure server: %s Action requested: %d\n", server_config->address, (int)server_config->action);

      if((int)server_config->action == 0)
      {
        // resize array
        server_count++;
        server_ports = realloc(server_ports, sizeof(char*) * server_count);
        server_size = realloc(server_size, sizeof(int*) * server_count);

	// add port to array and increment size
        server_ports[server_count-1] = malloc(strlen(server_config->address) + 1);
        strcpy(server_ports[server_count-1], server_config->address);
        server_size[server_count-1] = 0;
      }
      else
      {
	// find index
	int index = -1;
        for(int i=0; i<server_count; i++)
        {
	  if(strcmp(server_ports[i],server_config->address) == 0)
	   {
	     index = i;
	     break;
           }
        }

	// remove index and shrink array
        if(index != -1)
	{
	  for(int j = index; j < server_count - 1; j++) server_ports[j] = server_ports[j + 1];
          server_count--;
          server_ports = realloc(server_ports, sizeof(char*) * server_count);
          server_size = realloc(server_size, sizeof(int*) * server_count);
	}
      }

      fprintf (stderr,"Done\n");
      fprintf (stderr,"Server count: %d\n", server_count);
      
      Dkvs__Status result = DKVS__STATUS__INIT;
      result.status = 0;
      closure (&result, closure_data);
    }
}



static Dkvs__Router_Service the_router_service = DKVS__ROUTER__INIT(example__);


int main(int args, char** argv) {

  // initialize
  ht_create(500);
  const char *name = "4747";
  server_count = (args - 1);
  server_ports = malloc(server_count * sizeof(char*));
  server_size = malloc(server_count * sizeof(int));

  int j=0;
  for(int i=1; i<args; i++){
    server_ports[j] = malloc(strlen(argv[i] + 1));
    strcpy(server_ports[j], argv[i]);
    j++;
  }

  for(int i=0; i<server_count; i++){
    server_size[i] = 0;
  }
  
  // initialize server
  ProtobufC_RPC_Server *server;
  ProtobufC_RPC_AddressType address_type = PROTOBUF_C_RPC_ADDRESS_TCP;
  
  server = protobuf_c_rpc_server_new (address_type, name, (ProtobufCService *) &the_router_service, NULL);

  for (;;)
    protobuf_c_rpc_dispatch_run (protobuf_c_rpc_dispatch_default ());
  
  return 0;
}

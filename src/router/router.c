
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <openssl/md5.h>
#include <netinet/in.h>
#include <inttypes.h>

// protobuf-c headers
#include <protobuf-c/protobuf-c.h>
#include <protobuf-c-rpc/protobuf-c-rpc.h>
// Definitions for our protocol (under build/)
#include <protobuf/protocol.pb-c.h>

#define LIMIT 100000

static int server_count = 0;
static char** server_ports;
static int* server_size;
static char* localhost = "localhost";


typedef struct _Router__Db Router__Db;
struct _Router__Db
{
  char addr[15];
  char key[1024];
};

static Router__Db database[LIMIT];
int database_entries = 0;


void initialize_router(int args, char **argv){
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
}


// convert hash to integer
uint32_t hex2int(char *hex) {
  uint32_t val = 0;
  while (*hex) {
    // get current character then increment
    uint8_t byte = *hex++; 
    // transform hex character to the 4bit equivalent number, using the ascii table indexes
    if (byte >= '0' && byte <= '9') byte = byte - '0';
    else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
    else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;    
    // shift 4 to make space for new digit, and add the 4 bits of the new digit 
    val = (val << 4) | (byte & 0xF);
  }
  return val;
}


/* Hash a string for a particular key. */
uint32_t ht_hash( const char *key ) {

  unsigned char digest[16];
  const char* string = key;
  MD5_CTX context;
  MD5_Init(&context);
  MD5_Update(&context, string, strlen(string));
  MD5_Final(digest, &context);

  char md5string[33];
  
  for(int i = 0; i < 16; ++i)
    sprintf(&md5string[i*2], "%02x", (unsigned int)digest[i]);
  
  printf("%s\n", md5string);
  
  return hex2int(md5string);
}

void print_cache(){
  printf("Printing cache\n");
  for(int i = 0; i < database_entries; i++){
    printf("%s:%s\n", database[i].key, database[i].addr);
  }
}


//Check if the value is already stored on any node
char *scan_cache(char *key){
  for (int i = 0; i < database_entries; i++)
  {
    if (strcmp(database[i].key, key) == 0)
    {
      printf("Match found:%s:%s\n", database[i].key, key);
      return database[i].addr; 
    }
  }
  printf("Not found in cache\n");
  return NULL;
}

void add_to_cache(char *key, char *addr){
  printf("Adding to cache\n");
  if (database_entries < LIMIT)
  {
    strcpy(database[database_entries].key,key);
    strcpy(database[database_entries].addr, addr);
    database_entries++;
  }  
  print_cache(); 
}


char *select_server(char *key){
  printf("Searching for server\n");
  uint32_t key_id = ht_hash(key);

  for (int i = 0; i < server_count; i++)
  {
    printf("Server port:%s\n", server_ports[i]);
  }

  if (server_count == 1)
    return server_ports[0];
  
  if (key_id > ht_hash(server_ports[server_count - 1])){
    printf("Found:%s\n", server_ports[0]);
    return server_ports[0];
  }
  
  for (int i = 0; i < server_count; i++)
  {
    if (key_id <= ht_hash(server_ports[i]))
    {
      printf("Found:%s\n", server_ports[i]);
      return server_ports[i];
    }
  }
  
  return NULL;
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
      //char* addr = ht_get((char*)router_request->key.data);
      char *addr = scan_cache((char *)router_request->key.data);
      if(addr == NULL){
         //char* new_port =  get_server_with_memory();
	       char *new_port = select_server((char *)router_request->key.data);
         printf("Got port\n");
         if(new_port != NULL){
	         addr = malloc(strlen(localhost) + strlen(new_port) + 1);
	         strcpy(addr, localhost);
	         strcat(addr, ":");
	         strcat(addr, new_port);
	         strcat(addr,"\0");
	
	         //ht_set((char*)router_request->key.data, addr);
	         add_to_cache((char *)router_request->key.data, addr);
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
	      for(int j = index; j < server_count - 1; j++) 
          server_ports[j] = server_ports[j + 1];
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
  initialize_router(args, argv);

  const char *name = "4747";
    
  // initialize server
  ProtobufC_RPC_Server *server;
  ProtobufC_RPC_AddressType address_type = PROTOBUF_C_RPC_ADDRESS_TCP;
  
  server = protobuf_c_rpc_server_new (address_type, name, (ProtobufCService *) &the_router_service, NULL);

  for (;;)
    protobuf_c_rpc_dispatch_run (protobuf_c_rpc_dispatch_default ());
  
  return 0;
}

/************************************************
 * Copyright (c) IBM Corp. 2014
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *************************************************/

/*
 * Contributors:
 *     arayshu, lschneid - initial implementation
 */

#include <mpi.h>
#include <skv/common/skv_types.hpp>
#include <skv/common/skv_client_server_headers.hpp>
#include <skv/client/skv_client_server_conn.hpp>
#include <skv/common/skv_client_server_protocol.hpp>
#include <skv/server/skv_server_types.hpp>

#include <skv/server/skv_server_epstate_map.hpp>
#include <skv/server/skv_server_network_event_manager.hpp>

#include <skv/server/skv_server_event_source.hpp>

// include the implementations of the local kv backend
#include <skv/server/skv_local_kv_interface.hpp>

#include <skv/server/skv_server.hpp>

#if SKV_USE_VERBS
#include <rdma/rdma_cma.h>
#define ofed_bgvrnic_workaround() \
{ \
  struct rdma_event_channel *ch = rdma_create_event_channel (); \
  if( !ch )\
  { \
    std::cout << "rdma_create_event_channel failed. returned: " << (void*)ch << std::endl; \
    return -1; \
  } \
  rdma_destroy_event_channel( ch );  \
}
#else
#define ofed_bgvrnic_workaround()
#endif


#if defined(USE_MTRACE)
#include <mcheck.h>
#endif

skv_server_t Server;

int 
main(int argc, char **argv) 
{ 
#if defined(USE_MTRACE)
    mtrace() ;
#endif

  ofed_bgvrnic_workaround();

  /************************************************************
   * Initialize MPI
   ***********************************************************/
  MPI_Init( &argc, &argv );

  int rc = 0;
  int Rank;
  int NodeCount;
  MPI_Comm_rank( MPI_COMM_WORLD, &Rank );
  MPI_Comm_size( MPI_COMM_WORLD, &NodeCount );
  /***********************************************************/

  FxLogger_Init( argv[ 0 ], Rank );

  int op;
  std::string SKV_CONFIG_FILE_ARG;

  while( (op = getopt( argc, argv, "hc:C" )) != -1 )
  {
    char *endp;
    switch( op )
    {
      default:
        rc = EINVAL;
      case 'h':
      {
        std::cout << "USAGE: " << argv[ 0 ] << std::endl;
        std::cout << "  Arguments:\n";
        std::cout << "  -c <config>   : path and name of config file (default: search /etc/skv_server.conf and ~/.skv_server.conf)\n";
        std::cout << "  -C            : startup from checkpoint - load existing data from a file (not yet implemented)\n";
        std::cout << "  -h            : print this help\n";
        std::cout << "  " << std::endl;
        return rc;
      }
      case 'c':
        SKV_CONFIG_FILE_ARG.assign( optarg );
        break;
      case 'C':
        std::cout << " Checkpoint restart coming soon..." << std::endl;
        return ENOSYS;
    }

  }

  pkTraceServer::Init();

  skv_configuration_t *config = skv_configuration_t::GetSKVConfiguration( SKV_CONFIG_FILE_ARG.c_str() );

  // Clear the skv server ready file
  if( Rank == 0 )
  {
    rc = unlink( config->GetServerReadyFile() );

    BegLogLine( rc == 0 )
      << "skv_server_main::main():: Cleared skv server ready file: "
      << config->GetServerReadyFile()
      << EndLogLine;
  }

  BegLogLine( 1 )
    << "skv_server_main::main():: Entering... "
    << " argc: " << argc
    << " Rank: " << Rank
    << " NodeCount: " << NodeCount
    << EndLogLine;

  if( argc == 2 )
  {
    Server.Init( Rank, NodeCount, 0, argv[1] );
  }
  else
    Server.Init( Rank, NodeCount, 0, NULL );

  BegLogLine( 1 )
    << "skv_server_main::main():: Finished with Init(), calling Run() "
    << EndLogLine;

  MPI_Barrier( MPI_COMM_WORLD );

  BegLogLine( 1 )
    << "skv_server_main::main():: Server is ready!"
    << EndLogLine;

  if( Rank == 0 )
  {
    rc = open( config->GetServerReadyFile(),
    O_CREAT | O_TRUNC | O_RDWR,
                   S_IROTH | S_IWOTH );

    StrongAssertLogLine( rc != -1 )
      << "skv_server_main::main():: ERROR: Failed to create skv server ready file: "
      << config->GetServerReadyFile()
      << " errno: " << errno
      << EndLogLine;

    BegLogLine( 1 )
      << "skv_server_main::main():: Created skv server ready file: "
      << config->GetServerReadyFile()
      << EndLogLine;
  }

  Server.Run();

  return rc;
};

/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

/*!
\file PlusServer.cxx 
\brief Start Plus OpenIGTLink server for broadcasting selected IGTL messages. 
If testing enabled this program tests Plus server and Plus client. The communication in this test
happens between two threads. In real life, it happens between two programs.
*/ 

#include "PlusConfigure.h"
#include "vtkDataCollector.h"
#include "vtkOpenIGTLinkVideoSource.h"
#include "vtkPlusBuffer.h"
#include "vtkPlusChannel.h"
#include "vtkPlusDataSource.h"
#include "vtkPlusOpenIGTLinkServer.h"
#include "vtkSmartPointer.h"
#include "vtkTransformRepository.h"
#include "vtksys/CommandLineArguments.hxx"

// For catching Ctrl-C
#include <csignal>
#include <cstdlib>
#include <cstdio>

// Connect/disconnect clients to server for testing purposes
PlusStatus ConnectClients( int listeningPort, std::vector< vtkSmartPointer<vtkOpenIGTLinkVideoSource> >& testClientList, int numberOfClientsToConnect, vtkSmartPointer<vtkXMLDataElement> configRootElement ); 
PlusStatus DisconnectClients( std::vector< vtkSmartPointer<vtkOpenIGTLinkVideoSource> >& testClientList );

// Forward declare signal handler
void SignalInterruptHandler(int s);
static bool neverStop;

int main( int argc, char** argv )
{
#ifndef _WIN32
  // Prevent crash on linux when a client is disconnected
  // (socket->Send to a disconnected client generates a SIGPIPE signal that crashes the application if not handled
  // or explicitly ignored)
  signal(SIGPIPE, SIG_IGN);
#endif

  // Check command line arguments.
  std::string inputConfigFileName;
  std::string testingConfigFileName;
  int verboseLevel = vtkPlusLogger::LOG_LEVEL_UNDEFINED;
  double runTime = 0;

  const int numOfTestClientsToConnect = 5; // only if testing is enabled S

  vtksys::CommandLineArguments args;
  args.Initialize( argc, argv );

  args.AddArgument( "--config-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputConfigFileName, "Name of the input configuration file." );
  args.AddArgument( "--running-time", vtksys::CommandLineArguments::EQUAL_ARGUMENT,&runTime, "Server running time period in seconds. If the parameter is not defined or 0 then the server runs infinitely." );
  args.AddArgument( "--verbose", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &verboseLevel, "Verbose level (1=error only, 2=warning, 3=info, 4=debug, 5=trace)" );
  args.AddArgument( "--testing-config-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &testingConfigFileName, "Enable testing mode (testing PlusServer functionality by running a few OpenIGTLink clients)" );

  if ( !args.Parse() )
  {
    std::cerr << "Problem parsing arguments." << std::endl;
    std::cout << "Help: " << args.GetHelp() << std::endl;
    exit(EXIT_FAILURE); 
  }
  
  vtkPlusLogger::Instance()->SetLogLevel( verboseLevel );

  if ( inputConfigFileName.empty() )
  {
    LOG_ERROR("--config-file argument is required!"); 
    std::cout << "Help: " << args.GetHelp() << std::endl;
    exit(EXIT_FAILURE); 
  }

  // Create Plus OpenIGTLink server.
  LOG_DEBUG( "Initializing Plus OpenIGTLink server... " );
  vtkSmartPointer< vtkPlusOpenIGTLinkServer > server = vtkSmartPointer< vtkPlusOpenIGTLinkServer >::New();
  PlusStatus status=server->Start(inputConfigFileName);

  double startTime = vtkAccurateTimer::GetSystemTime(); 

  // *************************** Testing **************************
  std::vector< vtkSmartPointer<vtkOpenIGTLinkVideoSource> > testClientList; 
  if ( !testingConfigFileName.empty() )
  {
    std::string configFilePath=vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationPath(testingConfigFileName);
    // Read main configuration file
    vtkSmartPointer<vtkXMLDataElement> configRootElement = vtkSmartPointer<vtkXMLDataElement>::Take(
      vtkXMLUtilities::ReadElementFromFile(configFilePath.c_str()));
    if (configRootElement == NULL)
    {  
      LOG_ERROR("Unable to read tes configuration from file " << configFilePath.c_str()); 
      DisconnectClients( testClientList );
      server->Stop(); 
      exit(EXIT_FAILURE);
    }
    // Connect clients to server 
    if ( ConnectClients( server->GetListeningPort(), testClientList, numOfTestClientsToConnect, configRootElement ) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to connect clients to PlusServer!"); 
      DisconnectClients( testClientList );
      server->Stop(); 
      exit(EXIT_FAILURE);
    }
    vtkAccurateTimer::Delay( 1.0 ); // make sure the threads have some time to connect regardless of the specified runTime
    LOG_INFO("Clients are connected");
  }
  // *************************** End of testing **************************

#ifdef _WIN32
  std::cout << "Press Q or Ctrl-C to quit:" << std::endl;
#else
  std::cout << "Press Ctrl-C to quit:" << std::endl;
#endif
  // Set up signal catching
  signal(SIGINT, SignalInterruptHandler);

  neverStop = (runTime==0.0);

  // Run server until requested 
  const double commandQueuePollIntervalSec=0.010;
  while ( (vtkAccurateTimer::GetSystemTime() < startTime + runTime) || (neverStop) )
  {
    server->ProcessPendingCommands();
#ifdef _WIN32
    // Need to process messages because some devices (such as the vtkWin32VideoSource2) require event processing
    MSG Msg;
    while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&Msg);
      DispatchMessage(&Msg);
    }

    if( GetAsyncKeyState('Q') || GetAsyncKeyState('q') )
    {
      runTime = 0.0;
      neverStop = false;
    }
    Sleep(commandQueuePollIntervalSec*1000); // give a chance to other threads to get CPU time now
#else
    usleep(commandQueuePollIntervalSec * 1000000);
#endif
  }

  // *************************** Testing **************************
  if ( !testingConfigFileName.empty() )
  {
    LOG_INFO("Requested testing time elapsed");
    // make sure all the clients are still connected 
    int numOfActuallyConnectedClients=server->GetNumberOfConnectedClients();
    if ( numOfActuallyConnectedClients != numOfTestClientsToConnect )
    {
      LOG_ERROR("Number of connected clients to PlusServer doesn't match the requirements (" 
        << numOfActuallyConnectedClients << " out of " << numOfTestClientsToConnect << ")."); 
      DisconnectClients( testClientList );
      server->Stop(); 
      exit(EXIT_FAILURE);
    }

    // Disconnect clients from server
    LOG_INFO("Disconnecting clients...");
    if ( DisconnectClients( testClientList ) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to disconnect clients from PlusServer!"); 
      server->Stop(); 
      exit(EXIT_FAILURE);
    }
    LOG_INFO("Clients are disconnected");
  }
  // *************************** End of testing **************************


  server->Stop();
  
  if ( !testingConfigFileName.empty() )
  {
    LOG_INFO("Test is successfully completed");
  }

  return EXIT_SUCCESS;
}

// -------------------------------------------------
PlusStatus ConnectClients( int listeningPort, std::vector< vtkSmartPointer<vtkOpenIGTLinkVideoSource> >& testClientList, int numberOfClientsToConnect, vtkSmartPointer<vtkXMLDataElement> configRootElement )
{
  if (configRootElement==NULL)
  {
    LOG_ERROR("PlusServer client configuration is missing");
    return PLUS_FAIL;
  }

  int numberOfErrors = 0; 

  // Clear test client list 
  testClientList.clear(); 

  for ( int i = 0; i < numberOfClientsToConnect; ++i )
  {
    vtkSmartPointer<vtkOpenIGTLinkVideoSource> client = vtkSmartPointer<vtkOpenIGTLinkVideoSource>::New(); 
    client->SetDeviceId("OpenIGTLinkVideoSenderDevice");
    client->ReadConfiguration(configRootElement);
    client->SetServerAddress("localhost");
    client->SetServerPort(listeningPort); 
    if( client->OutputChannelCount() == 0 )
    {
      LOG_ERROR("No output channels in openIGTLink client.");
      ++numberOfErrors;
      continue;
    }
    vtkPlusChannel* aChannel = *(client->GetOutputChannelsStart());
    vtkPlusDataSource* aSource(NULL);
    if( aChannel->GetVideoSource(aSource) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to retrieve the video source.");
      continue;
    }
    client->SetBufferSize( *aChannel, 10 ); 
    client->SetMessageType( "TrackedFrame" ); 
    aSource->SetPortImageOrientation( US_IMG_ORIENT_MF );

    if ( client->Connect() != PLUS_SUCCESS )
    {
      LOG_ERROR("Client #" << i+1 << " couldn't connect to server."); 
      ++numberOfErrors;
      continue; 
    }

    LOG_DEBUG("Client #" << i+1 << " successfully connected to server!"); 

    if ( client->StartRecording() != PLUS_SUCCESS )
    {
      LOG_ERROR("Client #" << i+1 << " couldn't start recording frames."); 
      client->Disconnect(); 
      ++numberOfErrors;
      continue;
    }

    // Add connected client to list
    testClientList.push_back(client); 
  }

  return ( numberOfErrors == 0 ? PLUS_SUCCESS : PLUS_FAIL ); 
}

// -------------------------------------------------
PlusStatus DisconnectClients( std::vector< vtkSmartPointer<vtkOpenIGTLinkVideoSource> >& testClientList )
{
  int numberOfErrors = 0; 
  for ( int i = 0; i < testClientList.size(); ++i )
  {
    if ( testClientList[i]->StopRecording() != PLUS_SUCCESS )
    {
      LOG_ERROR("Client #" << i+1 << " failed to stop recording"); 
      ++numberOfErrors;
    }

    if ( testClientList[i]->Disconnect() != PLUS_SUCCESS )
    {
      LOG_ERROR("Client #" << i+1 << " failed to disconnect from server"); 
      ++numberOfErrors;
      continue;
    }

    LOG_DEBUG("Client #" << i+1 << " successfully disconnected from server!"); 
  }

  return ( numberOfErrors == 0 ? PLUS_SUCCESS : PLUS_FAIL ); 
}

// -------------------------------------------------
void SignalInterruptHandler(int s)
{
  neverStop = false;
}
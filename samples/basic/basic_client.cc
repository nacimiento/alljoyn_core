/**
 * @file
 * @brief  Sample implementation of an AllJoyn client.
 */

/******************************************************************************
 * $Revision: 15/11 $
 *
 * Copyright 2009-2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/
#include <qcc/platform.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <vector>

#include <qcc/String.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/version.h>
#include <alljoyn/AllJoynStd.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <Status.h>

using namespace std;
using namespace qcc;
using namespace ajn;

/** Static top level message bus object */
static BusAttachment* g_msgBus = NULL;

/*constants*/
static const char* SERVICE_NAME = "org.alljoyn.Bus.method_sample";
static const char* SERVICE_PATH = "/method_sample";


/** Signal handler */
static void SigIntHandler(int sig)
{
    if (NULL != g_msgBus) {
        QStatus status = g_msgBus->Stop(false);
        if (ER_OK != status) {
            printf("BusAttachment::Stop() failed\n");
        }
    }
    exit(0);
}

/** AllJoynListener receives discovery events from AllJoyn */
class MyBusListener : public BusListener {
  public:
    void FoundName(const char* name, const char* guid, const char* namePrefix, const char* busAddress)
    {
        if (0 == strcmp(name, SERVICE_NAME)) {
            printf("FoundName(name=%s, guid=%s, addr=%s)\n", name, guid, busAddress);
            /* We found a remote bus that is advertising bbservice's well-known name so connect to it */
            uint32_t disposition;
            QStatus status = g_msgBus->ConnectToRemoteBus(busAddress, disposition);
            if ((ER_OK != status) || (ALLJOYN_CONNECT_REPLY_SUCCESS != disposition)) {
                printf("ConnectToRemoteBus failed (status=%s, disposition=%d)", QCC_StatusText(status), disposition);
            }
        }
    }

    void NameOwnerChanged(const char* name, const char* previousOwner, const char* newOwner)
    {
        if (newOwner && (0 == strcmp(name, SERVICE_NAME))) {
            printf("NameOwnerChanged(%s, %s, %s)\n",
                   name,
                   previousOwner ? previousOwner : "null",
                   newOwner ? newOwner : "null");
        }
    }
};

/** Static bus listener */
static MyBusListener g_busListener;


/** Main entry point */
int main(int argc, char** argv, char** envArg)
{

#ifdef _WIN32
    WSADATA wsaData;
    WORD version = MAKEWORD(2, 0);
    int error = WSAStartup(version, &wsaData);
#endif

    QStatus status = ER_OK;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

#ifdef _WIN32
    qcc::String connectArgs = "tcp:addr=127.0.0.1,port=9955";
#else
    qcc::String connectArgs = "unix:abstract=bluebus";
#endif

    /* Create message bus */
    g_msgBus = new BusAttachment("myApp", true);

    /* Start the msg bus */
    if (ER_OK == status) {
        status = g_msgBus->Start();
        if (ER_OK != status) {
            printf("BusAttachment::Start failed\n");
        } else {
            printf("BusAttachment started.\n");
        }
    }

    /* Connect to the bus */
    if (ER_OK == status) {
        status = g_msgBus->Connect(connectArgs.c_str());
        if (ER_OK != status) {
            printf("BusAttachment::Connect(\"%s\") failed\n", connectArgs.c_str());
        } else {
            printf("BusAttchement connected to %s\n", connectArgs.c_str());
        }
    }

    /* Register a bus listener in order to get discovery indications */
    if (ER_OK == status) {
        g_msgBus->RegisterBusListener(g_busListener);
        printf("BusListener Registered.\n");
    }

    /* Begin discovery on the well-known name of the service to be called */
    if (ER_OK == status) {
        Message reply(*g_msgBus);
        const ProxyBusObject& alljoynObj = g_msgBus->GetAllJoynProxyObj();

        MsgArg serviceName("s", SERVICE_NAME);
        status = alljoynObj.MethodCall(ajn::org::alljoyn::Bus::InterfaceName,
                                       "FindName",
                                       &serviceName,
                                       1,
                                       reply,
                                       5000);
        if (ER_OK != status) {
            printf("%s.FindName failed\n", ajn::org::alljoyn::Bus::InterfaceName);
        } else {
            printf("org.alljoyn.Bus.FindName method called.\n");
        }
    }

    ProxyBusObject remoteObj;
    if (ER_OK == status) {
        remoteObj = ProxyBusObject(*g_msgBus, SERVICE_NAME, SERVICE_PATH);
        status = remoteObj.IntrospectRemoteObject();
        if (ER_OK != status) {
            printf("Introspection of %s (path=%s) failed\n", SERVICE_NAME, SERVICE_PATH);
            printf("Make sure the service is running before launching the client.\n");
        }
    }

    if (status == ER_OK) {
        Message reply(*g_msgBus);
        MsgArg inputs[2];
        inputs[0].Set("s", "Hello ");
        inputs[1].Set("s", "World!");
        status = remoteObj.MethodCall(SERVICE_NAME, "cat", inputs, 2, reply, 5000);
        if (ER_OK == status) {
            printf("%s.%s ( path=%s) returned \"%s\"\n", SERVICE_NAME, "cat",
                   SERVICE_PATH, reply->GetArg(0)->v_string.str);
        } else {
            printf("MethodCall on %s.%s failed", SERVICE_NAME, "cat");
        }
    }

    /* Stop the bus (not strictly necessary since we are going to delete it anyways) */
    if (g_msgBus) {
        QStatus s = g_msgBus->Stop();
        if (ER_OK != s) {
            printf("BusAttachment::Stop failed\n");
        }
    }

    /* Deallocate bus */
    if (g_msgBus) {
        BusAttachment* deleteMe = g_msgBus;
        g_msgBus = NULL;
        delete deleteMe;
    }

    printf("basic client exiting with status %d (%s)\n", status, QCC_StatusText(status));

    return (int) status;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsHttpProtocolConnection.h"
#include "nscore.h"
#include "nsIUrl.h"
#include "nsIHttpEventSink.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISocketTransportService.h"
#include "nsHttpProtocolHandler.h"
#include "nsITransport.h"
#include "nsIByteBufferInputStream.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////////////

nsHttpProtocolConnection::nsHttpProtocolConnection()
    : mHandler(nsnull), mUrl(nsnull), mEventSink(nsnull), mState(UNCONNECTED),
      mEventQueue(nsnull)
{
}

nsHttpProtocolConnection::~nsHttpProtocolConnection()
{
    NS_IF_RELEASE(mHandler);
    NS_IF_RELEASE(mUrl);
    NS_IF_RELEASE(mEventSink);
}

NS_IMPL_ADDREF(nsHttpProtocolConnection);
NS_IMPL_RELEASE(nsHttpProtocolConnection);

NS_IMETHODIMP
nsHttpProtocolConnection::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsIHttpProtocolConnection::GetIID()) ||
        aIID.Equals(nsIProtocolConnection::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsIHttpProtocolConnection*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIStreamListener::GetIID()) ||
        aIID.Equals(nsIStreamObserver::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

nsresult 
nsHttpProtocolConnection::Init(nsIUrl* url, nsISupports* eventSink, 
                               nsHttpProtocolHandler* handler,
                               PLEventQueue* eventQueue)
{
    nsresult rv;

    mHandler = handler;
    NS_ADDREF(mHandler);

    mUrl = url;
    NS_ADDREF(mUrl);

    rv = eventSink->QueryInterface(nsIHttpEventSink::GetIID(), (void**)&mEventSink);
    if (NS_FAILED(rv)) return rv;

    mEventQueue = eventQueue;
    
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsICancelable methods:

NS_IMETHODIMP
nsHttpProtocolConnection::Cancel(void)
{
    switch (mState) {
      case CONNECTED:
        break;
      case POSTING:
        break;
      default:
        return NS_ERROR_NOT_CONNECTED;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Suspend(void)
{
    switch (mState) {
      case CONNECTED:
        break;
      case POSTING:
        break;
      default:
        return NS_ERROR_NOT_CONNECTED;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Resume(void)
{
    switch (mState) {
      case CONNECTED:
        break;
      case POSTING:
        break;
      default:
        return NS_ERROR_NOT_CONNECTED;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolConnection methods:

NS_IMETHODIMP
nsHttpProtocolConnection::Open(void)
{
    nsresult rv = NS_OK;

    const char* host;
    rv = mUrl->GetHost(&host);
    if (NS_FAILED(rv)) return rv;

    PRInt32 port;
    rv = mUrl->GetPort(&port);
    if (NS_FAILED(rv)) return rv;
    
    rv = mHandler->GetTransport(host, port, &mTransport);
    mState = CONNECTED;
    return rv;
}

NS_IMETHODIMP
nsHttpProtocolConnection::GetContentType(char* *contentType)
{
    if (mState != CONNECTED)
        return NS_ERROR_NOT_CONNECTED;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::GetInputStream(nsIInputStream* *result)
{
    if (mState != CONNECTED)
        return NS_ERROR_NOT_CONNECTED;
    return mTransport->OpenInputStream(result);
}

NS_IMETHODIMP
nsHttpProtocolConnection::GetOutputStream(nsIOutputStream* *result)
{
    if (mState != CONNECTED)
        return NS_ERROR_NOT_CONNECTED;
    return mTransport->OpenOutputStream(result);
}

////////////////////////////////////////////////////////////////////////////////
// nsIHttpProtocolConnection methods:

NS_IMETHODIMP
nsHttpProtocolConnection::GetHeader(const char* header)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::AddHeader(const char* header, const char* value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::RemoveHeader(const char* header)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Get(void)
{
    nsresult rv;

    if (mState != CONNECTED)
        return NS_ERROR_NOT_CONNECTED;

    // Write the http request to the server. 
    // Note: We're doing synchronous writes here because it's
    // unlikely that our get request will fill the output
    // stream's buffer and block. 
    nsIOutputStream* out;
    rv = mTransport->OpenOutputStream(&out);
    if (NS_FAILED(rv)) return rv;

    PRUint32 written;
    rv = out->Write("GET ", 4, &written);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(written == 4, "write failed");

    const char* path;
    rv = mUrl->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    PRUint32 pathLen = nsCRT::strlen(path);
    rv = out->Write(path, pathLen, &written);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(written == pathLen, "write failed");
    rv = out->Write(" HTTP/1.1", 9, &written);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(written == 9, "write failed");

    rv = out->Flush();
    if (NS_FAILED(rv)) return rv;

    // start the state machine to read the response:
    rv = mTransport->AsyncRead(NS_STATIC_CAST(nsIProtocolConnection*, this),
                               mEventQueue, this);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

NS_IMETHODIMP
nsHttpProtocolConnection::GetByteRange(PRUint32 from, PRUint32 to)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Put(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Post(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsHttpProtocolConnection::OnStartBinding(nsISupports* context)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::OnStopBinding(nsISupports* context,
                                        nsresult aStatus,
                                        nsIString* aMsg)
{
    nsresult rv = NS_OK;
    switch (mState) {
      case CONNECTED:
        if (NS_SUCCEEDED(aStatus)) {
            // read the reply
            mState = WAITING_REPLY;
            rv = mTransport->AsyncRead(NS_STATIC_CAST(nsIProtocolConnection*, this),
                                       mEventQueue, this);
            if (NS_FAILED(rv)) return rv;
        }
        break;
        
      case WAITING_REPLY:
        // reply finished

//        rv = mHandler->ReleaseTransport(mTransport);
//        if (NS_FAILED(rv)) return rv;

        mState = UNCONNECTED;

        break;
      default:
        NS_NOTREACHED("bad state");
        break;
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsHttpProtocolConnection::OnDataAvailable(nsISupports* context,
                                          nsIInputStream *aIStream, 
                                          PRUint32 aLength)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////

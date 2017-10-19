/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
#include <string>
#include <cstdlib>

#include "FairMQMessageSHM.h"
#include "FairMQUnmanagedRegionSHM.h"
#include "FairMQLogger.h"
#include "FairMQShmCommon.h"

using namespace std;
using namespace fair::mq::shmem;

namespace bipc = boost::interprocess;

atomic<bool> FairMQMessageSHM::fInterrupted(false);
FairMQ::Transport FairMQMessageSHM::fTransportType = FairMQ::Transport::SHM;

FairMQMessageSHM::FairMQMessageSHM()
    : fMessage()
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(0)
    , fHandle()
    , fSize(0)
    , fLocalPtr(nullptr)
    , fRemoteRegion(nullptr)
{
    if (zmq_msg_init(&fMessage) != 0)
    {
        LOG(ERROR) << "failed initializing message, reason: " << zmq_strerror(errno);
    }
    fMetaCreated = true;
}

FairMQMessageSHM::FairMQMessageSHM(const size_t size)
    : fMessage()
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(0)
    , fHandle()
    , fSize(0)
    , fLocalPtr(nullptr)
    , fRemoteRegion(nullptr)
{
    InitializeChunk(size);
}

FairMQMessageSHM::FairMQMessageSHM(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
    : fMessage()
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(0)
    , fHandle()
    , fSize(0)
    , fLocalPtr(nullptr)
    , fRemoteRegion(nullptr)
{
    if (InitializeChunk(size))
    {
        memcpy(fLocalPtr, data, size);
        if (ffn)
        {
            ffn(data, hint);
        }
        else
        {
            free(data);
        }
    }
}

FairMQMessageSHM::FairMQMessageSHM(FairMQUnmanagedRegionPtr& region, void* data, const size_t size)
    : fMessage()
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(static_cast<FairMQUnmanagedRegionSHM*>(region.get())->fRegionId)
    , fHandle()
    , fSize(size)
    , fLocalPtr(data)
    , fRemoteRegion(nullptr)
{
    fHandle = (bipc::managed_shared_memory::handle_t)(reinterpret_cast<const char*>(data) - reinterpret_cast<const char*>(region->GetData()));

    if (zmq_msg_init_size(&fMessage, sizeof(MetaHeader)) != 0)
    {
        LOG(ERROR) << "failed initializing meta message, reason: " << zmq_strerror(errno);
    }
    else
    {
        MetaHeader header;
        header.fSize = size;
        header.fHandle = fHandle;
        header.fRegionId = fRegionId;
        memcpy(zmq_msg_data(&fMessage), &header, sizeof(MetaHeader));

        fMetaCreated = true;
    }
}

bool FairMQMessageSHM::InitializeChunk(const size_t size)
{
    while (!fHandle)
    {
        try
        {
            fLocalPtr = Manager::Instance().Segment()->allocate(size);
        }
        catch (bipc::bad_alloc& ba)
        {
            // LOG(WARN) << "Shared memory full...";
            this_thread::sleep_for(chrono::milliseconds(50));
            if (fInterrupted)
            {
                return false;
            }
            else
            {
                continue;
            }
        }
        fHandle = Manager::Instance().Segment()->get_handle_from_address(fLocalPtr);
    }

    fSize = size;

    if (zmq_msg_init_size(&fMessage, sizeof(MetaHeader)) != 0)
    {
        LOG(ERROR) << "failed initializing meta message, reason: " << zmq_strerror(errno);
        return false;
    }
    MetaHeader header;
    header.fSize = size;
    header.fHandle = fHandle;
    header.fRegionId = fRegionId;
    memcpy(zmq_msg_data(&fMessage), &header, sizeof(MetaHeader));

    fMetaCreated = true;

    return true;
}

void FairMQMessageSHM::Rebuild()
{
    CloseMessage();

    fQueued = false;

    if (zmq_msg_init(&fMessage) != 0)
    {
        LOG(ERROR) << "failed initializing message, reason: " << zmq_strerror(errno);
    }
    fMetaCreated = true;
}

void FairMQMessageSHM::Rebuild(const size_t size)
{
    CloseMessage();

    fQueued = false;

    InitializeChunk(size);
}

void FairMQMessageSHM::Rebuild(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
{
    CloseMessage();

    fQueued = false;

    if (InitializeChunk(size))
    {
        memcpy(fLocalPtr, data, size);
        if (ffn)
        {
            ffn(data, hint);
        }
        else
        {
            free(data);
        }
    }
}

void* FairMQMessageSHM::GetMessage()
{
    return &fMessage;
}

void* FairMQMessageSHM::GetData()
{
    if (fLocalPtr)
    {
        return fLocalPtr;
    }
    else
    {
        if (fRegionId == 0)
        {
            return Manager::Instance().Segment()->get_address_from_handle(fHandle);
        }
        else
        {
            if (!fRemoteRegion)
            {
                fRemoteRegion = FairMQUnmanagedRegionSHM::GetRemoteRegion(fRegionId);
            }
            fLocalPtr = reinterpret_cast<char*>(fRemoteRegion->get_address()) + fHandle;
            return fLocalPtr;
        }
    }
}

size_t FairMQMessageSHM::GetSize()
{
    return fSize;
}

void FairMQMessageSHM::SetMessage(void*, const size_t)
{
    // dummy method to comply with the interface. functionality not allowed in zeromq.
}

void FairMQMessageSHM::SetDeviceId(const string& /*deviceId*/)
{
    // fDeviceID = deviceId;
}

FairMQ::Transport FairMQMessageSHM::GetType() const
{
    return fTransportType;
}

void FairMQMessageSHM::Copy(const unique_ptr<FairMQMessage>& msg)
{
    if (!fHandle)
    {
        bipc::managed_shared_memory::handle_t otherHandle = static_cast<FairMQMessageSHM*>(msg.get())->fHandle;
        if (otherHandle)
        {
            if (InitializeChunk(msg->GetSize()))
            {
                memcpy(GetData(), msg->GetData(), msg->GetSize());
            }
        }
        else
        {
            LOG(ERROR) << "FairMQMessageSHM::Copy() fail: source message not initialized!";
        }
    }
    else
    {
        LOG(ERROR) << "FairMQMessageSHM::Copy() fail: target message already initialized!";
    }
}

void FairMQMessageSHM::CloseMessage()
{
    if (fHandle && !fQueued && fRegionId == 0)
    {
        Manager::Instance().Segment()->deallocate(Manager::Instance().Segment()->get_address_from_handle(fHandle));
        fHandle = 0;
    }

    if (fMetaCreated)
    {
        if (zmq_msg_close(&fMessage) != 0)
        {
            LOG(ERROR) << "failed closing message, reason: " << zmq_strerror(errno);
        }
    }
}

FairMQMessageSHM::~FairMQMessageSHM()
{
    CloseMessage();
}

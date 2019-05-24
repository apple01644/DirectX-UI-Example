#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

  //  FrameCB = std::make_unique<UploadBuffer<FrameConstants>>(device, 1, true);
    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, 32767, true);
	UICB = std::make_unique<UploadBuffer<UIConsts>>(device, 32767, true);
}

FrameResource::~FrameResource()
{

}
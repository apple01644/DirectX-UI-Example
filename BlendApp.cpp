
#include "Common/d3dApp.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
#include "Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "YTML1_1.hpp"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#include <random>
#include <time.h>
#include <DirectXMath.h>

const int gNumFrameResources = 3;
void OutputDebugStringA(const std::string& s) { OutputDebugStringA(s.c_str()); }

struct RenderItem
{
	RenderItem() = default;
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	int NumFramesDirty = gNumFrameResources;

	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	Count
};

class BlendApp : public D3DApp
{
public:
    BlendApp(HINSTANCE hInstance);
    BlendApp(const BlendApp& rhs) = delete;
    BlendApp& operator=(const BlendApp& rhs) = delete;
    ~BlendApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	virtual void OnKeyDown(WPARAM p) override;
	virtual void OnKeyUp(WPARAM p) override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildWavesGeometry();
	void BuildBoxGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList);

	void Brushing(const XMFLOAT2& pos, const float& range);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();


private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;
	std::unordered_map<std::string, ComPtr<ID3D12RootSignature>> mRootSignature;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::unordered_map<std::string, std::string> mStyle;

	std::unordered_map<std::string, std::vector<D3D12_INPUT_ELEMENT_DESC>> mInputLayout;
 
	// List of all the render items.
	std::unordered_map<std::string, std::unique_ptr<RenderItem>> mRitems;
	std::unique_ptr<UploadBuffer<VertexForMap>> MapVB;
	std::vector<VertexForMap> MapV;
	
    PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT2 mEyeOnMap = {0, 0};
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();
	XMMATRIX mViewProj;

    float mTheta = 1.5f*XM_PI;
    float mPhi = XM_PIDIV2 - 0.1f;
    float mRadius = 50.0f;

	UINT brushMode = 0;
	std::mt19937_64 mt = std::mt19937_64(time(nullptr));

    POINT mLastMousePos;
	UINT mLastMouseState = 0;

	YTML1_1::Tree mYTMLTree;
	size_t UICBSize = 0;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        BlendApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

BlendApp::BlendApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

BlendApp::~BlendApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool BlendApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	 
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildWavesGeometry();
	BuildBoxGeometry();
	BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void BlendApp::OnResize()
{
    D3DApp::OnResize();

	mYTMLTree->eid = 0;
	mYTMLTree->size = { (float)mClientWidth, (float)mClientHeight };
	mYTMLTree->flags = 0;

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void BlendApp::Update(const GameTimer& gt)
{
	if ((GetKeyState(VK_LBUTTON) & 0x100) != 0)
	{
		XMFLOAT4 pos = { -19.921875, 0.f, -19.921875, 1.f };
		XMVECTOR v = XMVector4Transform(XMVectorSet(pos.x, pos.y, pos.z, pos.w), mViewProj);
		XMStoreFloat4(&pos, v);
		XMFLOAT2 npos_start = { (pos.x / pos.w + 1.f) / 2.f * mClientWidth, (-pos.y / pos.w + 1.f) / 2.f * mClientHeight };

		pos = { 19.921875, 0.f, 19.921875, 1.f };
		v = XMVector4Transform(XMVectorSet(pos.x, pos.y, pos.z, pos.w), mViewProj);
		XMStoreFloat4(&pos, v);
		XMFLOAT2 npos_end = { (pos.x / pos.w + 1.f) / 2.f * mClientWidth, (-pos.y / pos.w + 1.f) / 2.f * mClientHeight };

		XMFLOAT4 rect = { npos_start.x, npos_start.y, npos_end.x - npos_start.x, npos_end.y - npos_start.y };
		if (rect.z < 0) {
			rect.x += rect.z;
			rect.z = -rect.z;
		}
		if (rect.w < 0) {
			rect.y += rect.w;
			rect.w = -rect.w;
		}

		if (mLastMousePos.x >= rect.x && mLastMousePos.y >= rect.y && mLastMousePos.x <= rect.x + rect.z && mLastMousePos.y <= rect.y + rect.w) {
			XMFLOAT2 nmp = { (mLastMousePos.x - rect.x) / rect.z, 1 - (mLastMousePos.y - rect.y) / rect.w };

			Brushing(nmp, 9);
		}
	}

    OnKeyboardInput(gt);
	UpdateCamera(gt);

    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);

	for (size_t i = 0; i < MapV.size(); ++i) {
		auto& v = MapV[i];

		MapVB->CopyData(i, v);
	}
}

void BlendApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["UI"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature["Map"].Get());

	

    DrawRenderItems(mCommandList.Get());//, mRitemLayer[(int)RenderLayer::Opaque]

	///mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	///DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	///mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	///DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;


    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void BlendApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		bool run = true;
		YTML1_1::RawLoopTree_RL(
			[&](YTML1_1::Element& e, bool& run) {
				if (e.flags & ElementFlag::Enable)
				{
					if (x >= e.size_in_display.x && y >= e.size_in_display.y && x <= e.size_in_display.x + e.size_in_display.w && y <= e.size_in_display.y + e.size_in_display.h)
					{
						e.background_color = (XMFLOAT4)Colors::Red;
						run = false;
					}
				}
			}
		, mYTMLTree, run);
	}

    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void BlendApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	//if ((btnState & MK_LBUTTON) != 0)
	{
		bool run = true;
		YTML1_1::RawLoopTree_RL(
			[&](YTML1_1::Element& e, bool& run) {
				if (e.flags & ElementFlag::Enable)
				{
					if (x >= e.size_in_display.x && y >= e.size_in_display.y && x <= e.size_in_display.x + e.size_in_display.w && y <= e.size_in_display.y + e.size_in_display.h)
					{
						e.background_color = (XMFLOAT4)Colors::Blue;
						run = false;
					}
				}
			}
		, mYTMLTree, run);
	}

    ReleaseCapture();
}

void BlendApp::Brushing(const XMFLOAT2& pos, const float& range)
{
	size_t min_x = 0, max_x = 256, min_y = 0, max_y = 256;

	if (pos.x * 255 - range > 0) min_x = (size_t)(pos.x * 255 - range);
	if (pos.y * 255 - range > 0) min_y = (size_t)(pos.y * 255 - range);

	if (pos.x * 255 + range < 256) max_x = (size_t)(pos.x * 255 + range);
	if (pos.y * 255 + range < 256) max_y = (size_t)(pos.y * 255 + range);

	for (size_t _x = min_x; _x < max_x; ++_x)
	{
		for (size_t _y = min_y; _y < max_y; ++_y)
		{
			auto& v = MapV[_y + _x * 256];
			float dist = sqrtf(powf(v.x - pos.x * 255, 2) + powf(v.y - pos.y * 255, 2));
			if (dist <= 9)
			{
				float* geo = (float*)& v.Geo;
				geo[brushMode] += 1.f - dist / range;
				float all = 0;
				for (int i = 0; i < MapTexture::size; ++i) all += geo[i];
				for (int i = 0; i < MapTexture::size; ++i) geo[i] /= all;
			}
		}
	}
}

void BlendApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
	mLastMouseState = btnState;
}

void BlendApp::OnKeyDown(WPARAM p) 
{
	switch (p) {
	case '1':
		brushMode = 0;
		break;
	case '2':
		brushMode = 1;
		break;
	case '3':
		brushMode = 2;
		break;
	case '4':
		brushMode = 3;
		break;
	}
}
void BlendApp::OnKeyUp(WPARAM) {}
 
void BlendApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('A')) mEyeOnMap.x -= 1.f * gt.DeltaTime() * mRadius;
	if (GetAsyncKeyState('D')) mEyeOnMap.x += 1.f * gt.DeltaTime() * mRadius;
	if (GetAsyncKeyState('W')) mEyeOnMap.y += 1.f * gt.DeltaTime() * mRadius;
	if (GetAsyncKeyState('S')) mEyeOnMap.y -= 1.f * gt.DeltaTime() * mRadius;
	MathHelper::Clamp(mEyeOnMap.x, -127.5f / 256.f * 40.f, 127.5f / 256.f * 40.f);
	MathHelper::Clamp(mEyeOnMap.y, -127.5f / 256.f * 40.f, 127.5f / 256.f * 40.f);

}
 
void BlendApp::UpdateCamera(const GameTimer& gt)
{
	mEyePos.x = mEyeOnMap.x;
	mEyePos.z = -0.000001f * mRadius + mEyeOnMap.y;
	mEyePos.y = mRadius;

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorSet(mEyeOnMap.x, 0.f, mEyeOnMap.y, 0.f);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void BlendApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if(tu >= 1.0f)
		tu -= 1.0f;

	if(tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}

void BlendApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e.second->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e.second->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e.second->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e.second->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e.second->NumFramesDirty--;
		}
	}
	auto currUICB = mCurrFrameResource->UICB.get();

	size_t i = 0;

	YTML1_1::RunYTML1_1(mYTMLTree,
		[&](YTML1_1::Element & e, bool& run) {
			if (e.flags & ElementFlag::Enable)
			{
				//Border and Body
				if (e.border.left != 0 || e.border.top != 0 || e.border.bottom != 0 || e.border.right != 0)
				{
					UIConsts c;
					XMStoreFloat4x4(&c.World,
						XMMatrixScaling(e.size_in_display.w, e.size_in_display.h, 0) +
						XMMatrixTranslation(e.size_in_display.x, e.size_in_display.y, 0)
					);

					c.Color = e.border_color;
					currUICB->CopyData(i++, c);

					XMStoreFloat4x4(&c.World,
						XMMatrixScaling(e.size_in_display.w - e.border.left - e.border.right, e.size_in_display.h - e.border.top - e.border.bottom, 0) +
						XMMatrixTranslation(e.size_in_display.x + e.border.left, e.size_in_display.y + e.border.top, 0)
					);

					c.Color = e.background_color;
					currUICB->CopyData(i++, c);
				}
				else
				//Only Body
				{
					UIConsts c;
					XMStoreFloat4x4(&c.World,
						XMMatrixScaling(e.size_in_display.w, e.size_in_display.h, 0) +
						XMMatrixTranslation(e.size_in_display.x, e.size_in_display.y, 0)
					);

					c.Color = e.background_color;
					currUICB->CopyData(i++, c);
				}

			}
		}
	);
	
	UICBSize = i;
}

void BlendApp::UpdateMaterialCBs(const GameTimer& gt)
{
	int i = 0;
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			mMainPassCB.gMaterial[i].DiffuseAlbedo = mat->DiffuseAlbedo;
			mMainPassCB.gMaterial[i].FresnelR0 = mat->FresnelR0;
			mMainPassCB.gMaterial[i].Roughness = mat->Roughness;
			XMStoreFloat4x4(&mMainPassCB.gMaterial[i].MatTransform, XMMatrixTranspose(matTransform));

			

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
		++i;
	}
}

void BlendApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	mViewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(mViewProj), mViewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(mViewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	/*mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.9f, 0.9f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };*/


	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);

}

void BlendApp::LoadTextures()
{
	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"Textures/plain.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"Textures/water.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"Textures/mountain.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));
	


	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
}

void BlendApp::BuildRootSignature()
{

	auto staticSamplers = GetStaticSamplers();
	{

		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 0);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[3];

		// Perfomance TIP: Order from most frequent to least frequent.
		slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[1].InitAsConstantBufferView(0);
		slotRootParameter[2].InitAsConstantBufferView(1);


		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
			(UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if (errorBlob != nullptr) ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());

		ThrowIfFailed(hr);

		ThrowIfFailed(md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignature["Map"].GetAddressOf())));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);


		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
			(UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if (errorBlob != nullptr) ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());

		ThrowIfFailed(hr);

		ThrowIfFailed(md3dDevice->CreateRootSignature(
			1,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignature["UI"].GetAddressOf())));
	}
}

void BlendApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 3;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto grassTex = mTextures["grassTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto fenceTex = mTextures["fenceTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	srvDesc.Format = waterTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);


	srvDesc.Format = grassTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
}

void BlendApp::BuildShadersAndInputLayout()
{

	const D3D_SHADER_MACRO defines[] =
	{
		NULL, NULL
	};

	mShaders["MapVS"] = d3dUtil::CompileShader(L"Shaders\\Map.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["MapPS"] = d3dUtil::CompileShader(L"Shaders\\Map.hlsl", nullptr, "PS", "ps_5_1");
	
	mShaders["UIVS"] = d3dUtil::CompileShader(L"Shaders\\UI.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["UIPS"] = d3dUtil::CompileShader(L"Shaders\\UI.hlsl", nullptr, "PS", "ps_5_1");
	
    mInputLayout["Map"] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0 + 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12 + 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24 + 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "GEO_FIRST", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32 + 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "GEO_SECOND", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48 + 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
	mInputLayout["UI"] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }//,
		//{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void BlendApp::BuildWavesGeometry()
{
	std::vector<std::uint16_t> indices;

	for (UINT i = 0; i < 256; ++i)
	{
		for (UINT j = 0; j < 256; ++j)
		{
			VertexForMap v = VertexForMap(i, j, XMFLOAT3((i - 127.5f) / 256.f * 40.f, 0, (j - 127.5f) / 256.f * 40.f), XMFLOAT3(), XMFLOAT2(i / 255.f * 2, j / 255.f * 2));
			v.Geo._0 = 1;
			MapV.push_back(v);
		}
	}

	MapVB = std::make_unique<UploadBuffer<VertexForMap>>(md3dDevice.Get(), MapV.size(), false);

	for (UINT i = 0; i < 255; ++i)
	{
		for (UINT j = 0; j < 255; ++j)
		{
			indices.push_back(i * 256 + j);
			indices.push_back(i * 256 + j + 1);
			indices.push_back(i * 256 + j + 256);
			
			indices.push_back(i * 256 + j + 1);
			indices.push_back(i * 256 + j + 257);
			indices.push_back(i * 256 + j + 256);
		}
	}


	UINT vbByteSize = (UINT)MapV.size()*sizeof(VertexForMap);
	UINT ibByteSize = (UINT)indices.size()*sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";


	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	/*
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), MapV.data(), vbByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), MapV.data(), vbByteSize, geo->VertexBufferUploader);*/

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(VertexForMap);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void BlendApp::BuildBoxGeometry()
{
	{
		GeometryGenerator geoGen;
		GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);

		std::vector<Vertex> vertices(box.Vertices.size());
		for (size_t i = 0; i < box.Vertices.size(); ++i)
		{
			auto& p = box.Vertices[i].Position;
			vertices[i].Pos = p;
			vertices[i].Normal = box.Vertices[i].Normal;
			vertices[i].TexC = box.Vertices[i].TexC;
		}

		const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

		std::vector<std::uint16_t> indices = box.GetIndices16();
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		auto geo = std::make_unique<MeshGeometry>();
		geo->Name = "boxGeo";

		ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
		CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

		ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
		CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

		geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

		geo->VertexByteStride = sizeof(Vertex);
		geo->VertexBufferByteSize = vbByteSize;
		geo->IndexFormat = DXGI_FORMAT_R16_UINT;
		geo->IndexBufferByteSize = ibByteSize;

		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)indices.size();
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;

		geo->DrawArgs["box"] = submesh;

		mGeometries["boxGeo"] = std::move(geo);
	}
	{
		std::vector<UIPoint> vertices;
		vertices.push_back({ XMFLOAT2(0, 0) });
		vertices.push_back({ XMFLOAT2(0, 1) });
		vertices.push_back({ XMFLOAT2(1, 0) });
		vertices.push_back({ XMFLOAT2(1, 1) });
		const UINT vbByteSize = (UINT)vertices.size() * sizeof(UIPoint);

		std::vector<std::uint16_t> indices = {0, 1, 3, 0, 2, 3};
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		auto geo = std::make_unique<MeshGeometry>();
		geo->Name = "rect";

		ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
		CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

		ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
		CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

		geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

		geo->VertexByteStride = sizeof(UIPoint);
		geo->VertexBufferByteSize = vbByteSize;
		geo->IndexFormat = DXGI_FORMAT_R16_UINT;
		geo->IndexBufferByteSize = ibByteSize;

		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)indices.size();
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;

		geo->DrawArgs["rect"] = submesh;

		mGeometries["rect"] = std::move(geo);
	}
}

void BlendApp::BuildPSOs()
{
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc;
		ZeroMemory(&PsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		PsoDesc.InputLayout = { mInputLayout["Map"].data(), (UINT)mInputLayout["Map"].size() };
		PsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		PsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		PsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		PsoDesc.SampleMask = UINT_MAX;
		PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		PsoDesc.NumRenderTargets = 1;
		PsoDesc.RTVFormats[0] = mBackBufferFormat;
		PsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
		PsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
		PsoDesc.DSVFormat = mDepthStencilFormat;	

		PsoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["MapVS"]->GetBufferPointer()),
			mShaders["MapVS"]->GetBufferSize()
		};
		PsoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["MapPS"]->GetBufferPointer()),
			mShaders["MapPS"]->GetBufferSize()
		};
		PsoDesc.pRootSignature = mRootSignature["Map"].Get();
		ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&mPSOs["Map"])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc;
		ZeroMemory(&PsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		PsoDesc.InputLayout = { mInputLayout["UI"].data(), (UINT)mInputLayout["UI"].size() };
		PsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		PsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

		D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
		transparencyBlendDesc.BlendEnable = true;
		transparencyBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_SRC_ALPHA;
		transparencyBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_ALPHA;
		transparencyBlendDesc.BlendOp = D3D12_BLEND_OP::D3D12_BLEND_OP_ADD;

		transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND::D3D12_BLEND_ZERO;
		transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND::D3D12_BLEND_ONE;
		transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP::D3D12_BLEND_OP_ADD;

		transparencyBlendDesc.LogicOpEnable = false;
		transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP::D3D12_LOGIC_OP_NOOP;
		transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE::D3D12_COLOR_WRITE_ENABLE_ALL;

		PsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
		PsoDesc.BlendState.AlphaToCoverageEnable = false;
		PsoDesc.BlendState.IndependentBlendEnable = false;

		PsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		PsoDesc.SampleMask = UINT_MAX;
		PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		PsoDesc.NumRenderTargets = 1;
		PsoDesc.RTVFormats[0] = mBackBufferFormat;
		PsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
		PsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
		PsoDesc.DSVFormat = mDepthStencilFormat;
		PsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;
		//PsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		PsoDesc.InputLayout = { mInputLayout["UI"].data(), (UINT)mInputLayout["UI"].size() };
		PsoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["UIVS"]->GetBufferPointer()),
			mShaders["UIVS"]->GetBufferSize()
		};
		PsoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["UIPS"]->GetBufferPointer()),
			mShaders["UIPS"]->GetBufferSize()
		};


				PsoDesc.pRootSignature = mRootSignature["UI"].Get();
		ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&mPSOs["UI"])));
	}
}

void BlendApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1));
    }
	
	YTML1_1::ReadCSS("somestyle.css", mStyle);	

	std::vector<std::string> keys;
	
	size_t muid = 1;
	YTML1_1::ReadYTML1_1("sample.html", mYTMLTree, mStyle, muid);
}

void BlendApp::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;

	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
}

void BlendApp::BuildRenderItems()
{
    auto groundRitem = std::make_unique<RenderItem>();
	groundRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&groundRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	groundRitem->ObjCBIndex = 0;
	groundRitem->Mat = mMaterials["water"].get();
	groundRitem->Geo = mGeometries["waterGeo"].get();
	groundRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	groundRitem->IndexCount = groundRitem->Geo->DrawArgs["grid"].IndexCount;
	groundRitem->StartIndexLocation = groundRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	groundRitem->BaseVertexLocation = groundRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitems["GROUND"] = std::move(groundRitem);


    /*auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["landGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
*/

	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
	boxRitem->ObjCBIndex = 2;
	boxRitem->Mat = mMaterials["wirefence"].get();
	boxRitem->Geo = mGeometries["boxGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitems["BOX"] = std::move(boxRitem);
}

void BlendApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList)
{
	auto passCB = mCurrFrameResource->PassCB->Resource();

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT uiCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(UIConsts));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto UICB = mCurrFrameResource->UICB->Resource();

	{
		auto& ri = mRitems["GROUND"];

		mCommandList->SetPipelineState(mPSOs["Map"].Get());
		mCommandList->SetGraphicsRootSignature(mRootSignature["Map"].Get());
		mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

		ri->Geo->VertexBufferGPU = MapVB->Resource();

		mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex0(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex0.Offset(0, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

		mCommandList->SetGraphicsRootDescriptorTable(0, tex0);
		mCommandList->SetGraphicsRootConstantBufferView(1, objCBAddress);

		mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
	{
		const auto& geo = mGeometries.at("rect");
		mCommandList->SetPipelineState(mPSOs["UI"].Get());
		mCommandList->SetGraphicsRootSignature(mRootSignature["UI"].Get());
		mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
		
		mCommandList->IASetVertexBuffers(0, 1, &geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		//CD3DX12_GPU_DESCRIPTOR_HANDLE tex0(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		//mCommandList->SetGraphicsRootDescriptorTable(0, tex0);
		
		const auto& arg = geo->DrawArgs.begin()->second;
		for (size_t i = UICBSize - 1; i < UICBSize; --i)
		{
			mCommandList->SetGraphicsRootConstantBufferView(0, UICB->GetGPUVirtualAddress() + i * uiCBByteSize);
			mCommandList->DrawIndexedInstanced(arg.IndexCount, 1, arg.StartIndexLocation, arg.BaseVertexLocation, 0);
		}
	}
    // For each render item...
    /*for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }*/
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> BlendApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

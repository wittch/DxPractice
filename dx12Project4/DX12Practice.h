#pragma once

#include "DXBaise.h"
#include <Model.h>
#include "SimpleCamera.h"
#include "StepTimer.h"



using namespace DirectX;
using Microsoft::WRL::ComPtr;
class DX12Practice:public DXBaise
{
public:
    DX12Practice(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);

protected:



    // Viewport dimensions.
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    // Window bounds
    RECT m_windowBounds;

    // Whether or not tearing is available for fullscreen borderless windowed mode.
    bool m_tearingSupport;

    // Adapter info.
    bool m_useWarpDevice;

    // Override to be able to start without Dx11on12 UI for PIX. PIX doesn't support 11 on 12. 
    bool m_enableUI;

private:
    static const UINT FrameCount = 3;
    static const UINT TextureWidth = 256;
    static const UINT TextureHeight = 256;
    static const UINT TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.


    static const UINT CityRowCount = 15;
    static const UINT CityColumnCount = 8;
    static const UINT CityMaterialCount = CityRowCount * CityColumnCount;
    static const UINT CityMaterialTextureWidth = 64;
    static const UINT CityMaterialTextureHeight = 64;
    static const UINT CityMaterialTextureChannelCount = 4;
    static const bool UseBundles = true;
    static const float CitySpacingInterval;

    _declspec(align(256u)) struct SceneConstantBuffer
    {
        XMFLOAT4X4 World;
        XMFLOAT4X4 WorldView;
        XMFLOAT4X4 WorldViewProj;
        uint32_t   DrawMeshlets;
    };

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
    };

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device2> m_device;
    ComPtr<ID3D12Resource> m_depthStencil;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator[FrameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12PipelineState> m_depthOnlyPipelineState;

    ComPtr<ID3D12GraphicsCommandList6> m_commandList;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_cbvDataBegin;

    ComPtr<ID3D12Resource> m_constantBuffer;
    UINT m_rtvDescriptorSize;
    bool DepthBoundsTestSupported;

    // App resources.
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    ComPtr<ID3D12Resource> m_texture;

    // Synchronization objects.
    UINT m_frameIndex;
    UINT m_frameCounter;
    UINT m_frameNumber;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameCount];

    StepTimer m_timer;
    SimpleCamera m_camera;
    Model m_model;


    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();

private:
    static const wchar_t* c_meshFilename;
    static const wchar_t* c_meshShaderFilename;
    static const wchar_t* c_pixelShaderFilename;
    
};


#pragma once
#include "stdafx.h"
#include "DXBaiseHelper.h"
#include "DXBaise.h"


using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Frustum
{
public:
	Frustum();


    void CreateDeviceResources(ID3D12Device2* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat);
    void ReleaseResources();

    void Update(DirectX::FXMMATRIX viewProj, DirectX::XMVECTOR(&planes)[6]);
    void Draw(ID3D12GraphicsCommandList1* cmdList);

protected:
    _declspec(align(256u))
        struct Constants
    {
        DirectX::XMFLOAT4X4 ViewProj;
        DirectX::XMFLOAT4   Planes[6];
        DirectX::XMFLOAT4   LineColor;
    };

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
    };

private:
    ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

    Microsoft::WRL::ComPtr<ID3D12Resource>      m_constantResource;
    void* m_constantData;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;



    uint32_t                                    m_frameIndex;
};


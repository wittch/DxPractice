#include "Frustum.h"
#include "DXBaise.h"




namespace
{
    const wchar_t* s_drawFrustumMsFilename = L"SceneShaders.hlsl";
    const wchar_t* s_debugDrawPsFilename = L"SceneShaders.hlsl";

    std::wstring GetAssetFullPath(const wchar_t* path)
    {
        WCHAR assetsPath[512];
        GetAssetsPath(assetsPath, _countof(assetsPath));

        return std::wstring(assetsPath) + path;
    }
}





Frustum::Frustum() :
	m_constantData(nullptr),
	m_frameIndex(0)
{
}



void Frustum::CreateDeviceResources(ID3D12Device2* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat)
{


    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"SceneShaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"SceneShaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

        

        const CD3DX12_HEAP_PROPERTIES constantBufferHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(Constants) * 2);

        // Create the constant buffer
        ThrowIfFailed(device->CreateCommittedResource(
            &constantBufferHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &constantBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constantResource)
        ));

        ThrowIfFailed(m_constantResource->Map(0, nullptr, &m_constantData));
    }

    Vertex tmpVertices[] =
    {
        { { 0.00f,  0.25f  , 0.1f },    // Top
        { 1.0f,   0.0f,  0.0f,  1.0f } },        // Red
        { { 0.25f, -0.25f  , 0.9f },    // Right
        { 0.0f,   1.0f,  0.0f,  1.0f } },        // Green

        //{ { 0.25f, -0.25f * m_aspectRatio, 0.9f },    // Right
        //{ 0.0f,   1.0f,  0.0f,  1.0f } },        // Green
        { { -0.25f, -0.25f  , 0.5f },    // Left
        { 0.0f,   0.0f,  1.0f,  1.0f } },         // Blue

        //{ { -0.25f, -0.25f * m_aspectRatio, 0.5f },    // Left
        //{ 0.0f,   0.0f,  1.0f,  1.0f } },         // Blue
        { { 0.00f,  0.25f , 0.1f },    // Top
        { 1.0f,   0.0f,  0.0f,  1.0f } },        // Red

    };

    float fovy = 2.0f;

    std::vector<XMFLOAT3> vertices =
    {
        //*** near place
        {-0.25f, 0.25f, 0.25f },//left top
        {-0.25f, -0.25f, 0.25f}, // left bottom
        {0.25f, -0.25f, 0.0f},// right bottom
        {0.25f, 0.25f,0.0f},// right top


        //***far place
        {-0.25f * fovy, 0.25f * fovy, 0.25f },//left top
        {-0.25f * fovy, -0.25f * fovy, 0.25f}, // left bottom
        {0.25f * fovy, -0.25f * fovy, 0.0f},// right bottom
        {0.25f * fovy, 0.25f * fovy,0.0f},// right top

    };



    Vertex FrustumVertices[] =
    {
        //*** near place draw ccw
        {vertices[0],{1.0f,0.0f,0.0f,1.0f}},
        {vertices[1],{1.0f,0.0f,0.0f,1.0f}},

        {vertices[1],{1.0f,0.0f,0.0f,1.0f}},
        {vertices[2], {1.0f,0.0f,0.0f,1.0f}},

        {vertices[2],{1.0f,0.0f,0.0f,1.0f}},
        {vertices[3],{1.0f,0.0f,0.0f,1.0f}},

        {vertices[3], {1.0f,0.0f,0.0f,1.0f}},
        {vertices[0],{1.0f,0.0f,0.0f,1.0f}},


        //***far place draw ccw
        {vertices[4],{1.0f,0.0f,0.0f,1.0f}},
        {vertices[5],{1.0f,0.0f,0.0f,1.0f}},

        {vertices[5],{1.0f,0.0f,0.0f,1.0f}},
        {vertices[6], {1.0f,0.0f,0.0f,1.0f}},

        {vertices[6],{1.0f,0.0f,0.0f,1.0f}},
        {vertices[7],{1.0f,0.0f,0.0f,1.0f}},

        {vertices[7], {1.0f,0.0f,0.0f,1.0f}},
        {vertices[4],{1.0f,0.0f,0.0f,1.0f}},

        //***near to far
        {vertices[0],{1.0f,0.0f,0.0f,1.0f}},
        {vertices[4],{1.0f,0.0f,0.0f,1.0f}},

        {vertices[1],{1.0f,0.0f,0.0f,1.0f}},
        {vertices[5], {1.0f,0.0f,0.0f,1.0f}},

        {vertices[2],{1.0f,0.0f,0.0f,1.0f}},
        {vertices[6],{1.0f,0.0f,0.0f,1.0f}},

        {vertices[3], {1.0f,0.0f,0.0f,1.0f}},
        {vertices[7],{1.0f,0.0f,0.0f,1.0f}},
    };

    const UINT vertexBufferSize = sizeof(FrustumVertices);

    // Note: using upload heaps to transfer static data like vert buffers is not 
    // recommended. Every time the GPU needs it, the upload heap will be marshalled 
    // over. Please read up on Default Heap usage. An upload heap is used here for 
    // code simplicity and because there are very few verts to actually transfer.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));

    // Copy the triangle data to the vertex buffer.
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, FrustumVertices, sizeof(FrustumVertices));
    m_vertexBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view.
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vertexBufferSize;


}

void Frustum::ReleaseResources()
{
    m_rootSignature.Reset();
    m_pso.Reset();
    m_constantResource.Reset();
}

void Frustum::Update(DirectX::FXMMATRIX viewProj, DirectX::XMVECTOR(&planes)[6])
{
   
}

void Frustum::Draw(ID3D12GraphicsCommandList1* cmdList)
{
    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetGraphicsRootConstantBufferView(0, m_constantResource->GetGPUVirtualAddress() + sizeof(Constants) * m_frameIndex);

    cmdList->SetPipelineState(m_pso.Get());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    UINT VertexBufferSize = m_vertexBufferView.SizeInBytes;


    cmdList->DrawInstanced(VertexBufferSize, 1, 0, 0);
}

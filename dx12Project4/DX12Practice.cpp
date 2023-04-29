//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "DX12Practice.h"

std::unique_ptr<PrimitiveBatch<VertexPositionColor>>    g_Batch;

DX12Practice::DX12Practice(UINT width, UINT height, std::wstring name) :
    DXBaise(width, height, name),
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_frameNumber(0),
    m_rtvDescriptorSize(0)
{
}

void DX12Practice::OnInit()
{
    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void DX12Practice::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    // It is recommended to always use the tearing flag when it is available.
    swapChainDesc.Flags = m_tearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));

    if (m_tearingSupport)
    {
        
        ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
    }

    

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        // Describe and create a shader resource view (SRV) heap for the texture.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = 1;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

        // Describe and create a depth stencil view (DSV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void DX12Practice::LoadAssets()
{
    // Create the root signature.
    {
        //D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        //// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        //featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        //if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        //{
        //    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        //}

        //CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        //ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        //CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        //rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

        //D3D12_STATIC_SAMPLER_DESC sampler = {};
        //sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        //sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        //sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        //sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        //sampler.MipLODBias = 0;
        //sampler.MaxAnisotropy = 0;
        //sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        //sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        //sampler.MinLOD = 0.0f;
        //sampler.MaxLOD = D3D12_FLOAT32_MAX;
        //sampler.ShaderRegister = 0;
        //sampler.RegisterSpace = 0;
        //sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        //CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        //rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        //ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

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

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

         /*To bring up depth bounds feature, DX12 introduces a new concept to create pipeline state, called PSO stream.
         It is required to use PSO stream to enable depth bounds test.
        
         PSO stream is a more flexible way to extend the design of pipeline state. In this new concept, you can think 
         each subobject (e.g. Root Signature, Vertex Shader, or Pixel Shader) in the pipeline state is a token and the 
         whole pipeline state is a token stream. To create a PSO stream, you describe a set of subobjects required for rendering, and 
         then use the descriptor to create the a PSO. For any pipeline state subobject not found in the descriptor, 
         defaults will be used. Defaults will also be used if an old version of a subobject is found in the stream. For example, 
         an old DepthStencil State desc would not contain depth bounds test information so the depth bounds test value will  
         default to disabled.*/

        // Define the pipeline state for rendering a triangle with depth bounds test enabled.
        struct RENDER_WITH_DBT_PSO_STREAM
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS PS;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 DepthStencilState; // New depth stencil subobject with depth bounds test toggle
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } renderWithDBTPSOStream;

        // Wraps an array of render target format(s).
        D3D12_RT_FORMAT_ARRAY RTFormatArray = {};
        RTFormatArray.NumRenderTargets = 1;
        RTFormatArray.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        renderWithDBTPSOStream.RootSignature = m_rootSignature.Get();
        renderWithDBTPSOStream.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        renderWithDBTPSOStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        renderWithDBTPSOStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        renderWithDBTPSOStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        renderWithDBTPSOStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        renderWithDBTPSOStream.RTVFormats = RTFormatArray;

        // Create the descriptor of the PSO stream
        const D3D12_PIPELINE_STATE_STREAM_DESC renderWithDBTPSOStreamDesc = { sizeof(renderWithDBTPSOStream), &renderWithDBTPSOStream };

        // Check for the feature support of Depth Bounds Test
        D3D12_FEATURE_DATA_D3D12_OPTIONS2 Options = {};
        DepthBoundsTestSupported = SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &Options, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS2))) &&
            Options.DepthBoundsTestSupported;

        // Create a PSO with depth bounds test enabled (or disabled, based on the result of the feature query).
        CD3DX12_DEPTH_STENCIL_DESC1 depthDesc(D3D12_DEFAULT);
        depthDesc.DepthBoundsTestEnable = DepthBoundsTestSupported;
        depthDesc.DepthEnable = FALSE;
        renderWithDBTPSOStream.DepthStencilState = depthDesc;
        ThrowIfFailed(m_device->CreatePipelineState(&renderWithDBTPSOStreamDesc, IID_PPV_ARGS(&m_pipelineState)));

        // Create a PSO to prime depth only.
        struct DEPTH_ONLY_PSO_STREAM
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } depthOnlyPSOStream;

        depthOnlyPSOStream.RootSignature = m_rootSignature.Get();
        depthOnlyPSOStream.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        depthOnlyPSOStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        depthOnlyPSOStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        depthOnlyPSOStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        depthOnlyPSOStream.RTVFormats = RTFormatArray;

        const D3D12_PIPELINE_STATE_STREAM_DESC depthOnlyPSOStreamDesc = { sizeof(depthOnlyPSOStream), &depthOnlyPSOStream };
        ThrowIfFailed(m_device->CreatePipelineState(&depthOnlyPSOStreamDesc, IID_PPV_ARGS(&m_depthOnlyPipelineState)));

        //// Describe and create the graphics pipeline state object (PSO).
        //D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        //psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        //psoDesc.pRootSignature = m_rootSignature.Get();
        //psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        //psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        //psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        //psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        //psoDesc.DepthStencilState.DepthEnable = FALSE;
        //psoDesc.DepthStencilState.StencilEnable = FALSE;
        //psoDesc.SampleMask = UINT_MAX;
        //psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        //psoDesc.NumRenderTargets = 1;
        //psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32_FLOAT;
        //psoDesc.SampleDesc.Count = 1;
        //ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());


    // Create the vertex buffer.
    {

        Vertex tmpVertices[] = 
        {
            { { 0.00f,  0.25f * m_aspectRatio, 0.1f },    // Top
            { 1.0f,   0.0f,  0.0f,  1.0f } },        // Red
            { { 0.25f, -0.25f * m_aspectRatio, 0.9f },    // Right
            { 0.0f,   1.0f,  0.0f,  1.0f } },        // Green

            //{ { 0.25f, -0.25f * m_aspectRatio, 0.9f },    // Right
            //{ 0.0f,   1.0f,  0.0f,  1.0f } },        // Green
            { { -0.25f, -0.25f * m_aspectRatio, 0.5f },    // Left
            { 0.0f,   0.0f,  1.0f,  1.0f } },         // Blue

            //{ { -0.25f, -0.25f * m_aspectRatio, 0.5f },    // Left
            //{ 0.0f,   0.0f,  1.0f,  1.0f } },         // Blue
            { { 0.00f,  0.25f * m_aspectRatio, 0.1f },    // Top
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
        ThrowIfFailed(m_device->CreateCommittedResource(
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


    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;

    // Create an event handle to use for frame synchronization.
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    // Wait for the command list to execute; we are reusing the same command 
    // list in our main loop but for now, we just want to wait for setup to 
    // complete before continuing.
    WaitForPreviousFrame();
    }
}


// Update frame-based values.
void DX12Practice::OnUpdate()
{
   
}

// Render the scene.
void DX12Practice::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void DX12Practice::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void DX12Practice::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_depthOnlyPipelineState.Get()));
    //ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
   

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    UINT VertexBufferSize = m_vertexBufferView.SizeInBytes;

    // Render only the depth stencil view of the triangle to prime the depth value of the triangle
    m_commandList->DrawInstanced(VertexBufferSize, 1, 0, 0);
    
    // Move depth bounds so we can see they move. Depth bound test will test against DEST depth
    // that we primed previously
    const FLOAT f = 0;// 0.125f + sinf((m_frameNumber & 0x7F) / 127.f) * 0.125f;      // [0.. 0.25]
    if (DepthBoundsTestSupported)
    {
        m_commandList->OMSetDepthBounds(0.0f + f, 1.0f - f);
    }

    // Render the triangle with depth bounds
    m_commandList->SetPipelineState(m_pipelineState.Get());
    m_commandList->DrawInstanced(VertexBufferSize, 1, 0, 0);

    // Disable depth bounds on Direct3D 12 by resetting back to the default range
    if (DepthBoundsTestSupported)
    {
        m_commandList->OMSetDepthBounds(0.0f, 1.0f);
    }


    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}

void DX12Practice::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

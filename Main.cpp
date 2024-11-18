#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#include "d3dx12.h"

// Window procedure function to handle messages from the OS
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) { // If the window is destroyed, quit the application
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam); // Default message handling
}

// Function to create a window for rendering
HWND CreateRenderWindow(HINSTANCE hInstance, int width, int height) {
    // Define window class properties
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0, 0, hInstance, nullptr, nullptr, nullptr, nullptr, L"DX12Window", nullptr };
    RegisterClassEx(&wc); // Register window class

    // Create the window and return the handle
    HWND hwnd = CreateWindow(wc.lpszClassName, L"DirectX 12 Renderer", WS_OVERLAPPEDWINDOW, 100, 100, width, height, nullptr, nullptr, wc.hInstance, nullptr);

    ShowWindow(hwnd, SW_SHOWDEFAULT); // Show the window
    return hwnd;
}

// Microsoft::WRL::ComPtr is used for COM (Component Object Model) pointers with automatic reference counting
Microsoft::WRL::ComPtr<ID3D12Device> device; // Main interface for the Direct3D 12 device
Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue; // Queue for GPU commands

Microsoft::WRL::ComPtr<ID3D12Fence> fence;
UINT64 fenceValue = 0;
HANDLE fenceEvent; // Event handle for GPU synchronization

void InitD3D12(HWND hwnd) {
    // Enable debug layer for DirectX 12 in debug mode (useful for debugging graphics applications)
#if defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer(); // Enable debug validation
    }
#endif

    // Create the D3D12 device
    HRESULT deviceResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(deviceResult)) { // Check if device creation failed
        MessageBox(hwnd, L"Failed to create DirectX 12 device.", L"Error", MB_OK);
        exit(-1); // Exit the application if device creation failed
    }

    // Describe and create the command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // Direct command list for rendering commands

    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)); // Create the command queue

    // Create the fence after creating the Direct3D 12 device
    HRESULT fenceResult = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(fenceResult)) {
        // Handle error, fence creation failed
        return;
    }
    fenceValue = 1; // Initial fence value

    // Create an event handle to use for synchronization
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr) {
        // Handle error if event creation fails
        fenceResult = HRESULT_FROM_WIN32(GetLastError());
        return;
    }
}

// Swap chain for presenting rendered frames to the display
Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
UINT frameIndex; // Index of the current back buffer in the swap chain

void CreateSwapChain(HWND hwnd, int width, int height) {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory)); // Create a factory for creating DXGI objects like swap chains

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {}; // Describe swap chain properties
    swapChainDesc.BufferCount = 2; // Double buffering
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Format for colors in RGBA
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // Used as a render target
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Discards contents after presenting
    swapChainDesc.OutputWindow = hwnd; // The window to present to
    swapChainDesc.SampleDesc.Count = 1; // No multisampling
    swapChainDesc.Windowed = TRUE; // Windowed mode

    Microsoft::WRL::ComPtr<IDXGISwapChain> tempSwapChain;
    factory->CreateSwapChain(commandQueue.Get(), &swapChainDesc, &tempSwapChain); // Create the swap chain
    tempSwapChain.As(&swapChain); // Cast to IDXGISwapChain3

    frameIndex = swapChain->GetCurrentBackBufferIndex(); // Get the index of the current back buffer
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap; // Descriptor heap for render target views
Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[2]; // Render targets (2 for double buffering)
UINT rtvDescriptorSize; // Size of each RTV descriptor

void CreateRTVHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {}; // Describe the RTV descriptor heap
    rtvHeapDesc.NumDescriptors = 2; // Number of descriptors (one for each buffer)
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)); // Create the RTV descriptor heap
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // Get size of RTV descriptors

    // Create an RTV for each buffer in the swap chain
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < 2; i++) {
        swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])); // Get the buffer resource
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle); // Create RTV
        rtvHandle.Offset(1, rtvDescriptorSize); // Move to the next descriptor
    }
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator; // Allocator for command list memory
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList; // Command list for recording GPU commands

void CreateCommandList() {
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)); // Create command allocator
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)); // Create command list

    commandList->Close(); // Close command list (will be reset when used)
}

struct Vertex {
    DirectX::XMFLOAT3 position; // 3D position of the vertex
    DirectX::XMFLOAT4 color;    // RGBA color of the vertex
};

Vertex triangleVertices[] = {
    { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },  // Top vertex (Red)
    { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },  // Right vertex (Green)
    { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },  // Left vertex (Blue)
};

Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

void CreateVertexBuffer() {
    const UINT vertexBufferSize = sizeof(triangleVertices);

    // Create the vertex buffer resource in the GPU's default heap
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
    device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer));

    // Copy the triangle vertices to the vertex buffer
    UINT8* vertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU
    vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));
    memcpy(vertexDataBegin, triangleVertices, sizeof(triangleVertices));
    vertexBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = vertexBufferSize;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

void CreatePipelineState() {
    // Root signature description
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

    // Compile shaders
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
    D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vertexShader, nullptr);
    D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixelShader, nullptr);

    // Describe the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Describe and create the graphics pipeline state object (PSO)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
}

D3D12_VIEWPORT viewport;
D3D12_RECT scissorRect;

void InitializeViewportAndScissor(int width, int height) {
    // Set viewport dimensions (usually match the window size)
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // Define scissor rectangle (also typically matches the window size)
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(width);
    scissorRect.bottom = static_cast<LONG>(height);
}

void Render() {
    // Reset command allocator and command list
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), pipelineState.Get());

    // Transition the render target
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList->ResourceBarrier(1, &barrier);

    // Set render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Clear render target
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Set up pipeline state and vertex buffer
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->DrawInstanced(3, 1, 0, 0); // Draw 3 vertices as one triangle

    // Transition back to present
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    commandList->ResourceBarrier(1, &barrier);

    // Execute the command list
    commandList->Close();
    commandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(commandList.GetAddressOf()));

    // Present the frame
    swapChain->Present(1, 0);

    // Signal and increment the fence value to sync the frame
    const UINT64 currentFenceValue = fenceValue;
    commandQueue->Signal(fence.Get(), currentFenceValue);
    fenceValue++;

    // Wait until the previous frame is done
    if (fence->GetCompletedValue() < currentFenceValue) {
        fence->SetEventOnCompletion(currentFenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    frameIndex = swapChain->GetCurrentBackBufferIndex();
}

bool ProcessWindowMessages() {
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        // If a quit message is received, return false to stop the main loop
        if (msg.message == WM_QUIT) {
            return false;
        }
    }
    return true; // Continue running the application
}

void Cleanup() {
    // Wait for GPU to finish all work before releasing resources
    if (commandQueue && fence && fenceValue) {
        commandQueue->Signal(fence.Get(), fenceValue);

        // Ensure the fence has been reached
        if (fence->GetCompletedValue() < fenceValue) {
            HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (eventHandle) {
                fence->SetEventOnCompletion(fenceValue, eventHandle);
                WaitForSingleObject(eventHandle, INFINITE);
                CloseHandle(eventHandle);
            }
        }
    }

    // Release COM objects by resetting them (ComPtr handles their Release)
    vertexBuffer.Reset();
    commandAllocator.Reset();
    commandQueue.Reset();
    commandList.Reset();
    rtvHeap.Reset();
    swapChain.Reset();
    device.Reset();
    fence.Reset();
}

int main() {
    int width = 1600;
    int height = 900;

    HWND window = CreateRenderWindow(nullptr, width, height);
    InitD3D12(window);
    CreateSwapChain(window, width, height);
    CreateRTVHeap();
    CreateCommandList();
    CreateVertexBuffer();
    CreatePipelineState();
    InitializeViewportAndScissor(width, height);

    while (ProcessWindowMessages()) {
        Render();
    }

    Cleanup();
    return 0;
}

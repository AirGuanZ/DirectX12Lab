#include <d3d12.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>

#include <agz/d3d12/window/debugLayer.h>

AGZ_D3D12_BEGIN

void enableD3D12DebugLayer()
{
    ComPtr<ID3D12Debug> debugController;
    if(SUCCEEDED(D3D12GetDebugInterface(
        IID_PPV_ARGS(debugController.GetAddressOf()))))
    {
        debugController->EnableDebugLayer();
    }
    else
        OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");

    ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
    if(SUCCEEDED(DXGIGetDebugInterface1(
        0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
    {
        dxgiInfoQueue->SetBreakOnSeverity(
            DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        dxgiInfoQueue->SetBreakOnSeverity(
            DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
    }
}

AGZ_D3D12_END

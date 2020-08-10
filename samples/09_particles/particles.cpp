#include <random>

#include <agz/utility/file.h>
#include <agz/utility/mesh.h>

#include "./particles.h"

void AttractorMesh::loadPlane()
{
    Triangles triangles;
    triangles.push_back({
        { -1, 0, -1 },
        { -1, 0, +1 },
        { +1, 0, +1 }
    });
    triangles.push_back({
        { -1, 0, -1 },
        { +1, 0, +1 },
        { +1, 0, -1 }
    });

    setTriangles(triangles);
}

void AttractorMesh::loadFromFile(const std::string &filename)
{
    const auto tris = agz::mesh::load_from_file(filename);

    Triangles triangles;
    for(auto &t : tris)
    {
        triangles.push_back({
            t.vertices[0].position,
            t.vertices[1].position,
            t.vertices[2].position,
        });
    }

    setTriangles(triangles);
}

void AttractorMesh::sampleSurface(size_t samplesCnt, Vec3 *output) const
{
    std::default_random_engine rng{ std::random_device()() };
    std::uniform_real_distribution<float> dis(0, 1);

    for(size_t i = 0; i < samplesCnt; ++i)
    {
        const float u0 = dis(rng);
        const float u1 = dis(rng);
        const float u2 = dis(rng);
        const float u3 = dis(rng);

        const auto triangleIdx = triangleSampler_.sample(u0, u1);
        const auto biCoord = agz::math::distribution
            ::uniform_on_triangle(u2, u3);

        const Triangle &tri = triangles_[triangleIdx];

        const Vec3 pos = tri.a + biCoord.x * (tri.b - tri.a)
                               + biCoord.y * (tri.c - tri.a);

        output[i] = pos;
    }
}

ComPtr<ID3D12Resource> AttractorMesh::generateAttractorData(
    ID3D12Device *device,
    ResourceUploader &uploader,
    uint32_t attractorCount) const
{
    std::vector<Vec3> attractorPositions(attractorCount);
    sampleSurface(attractorCount, attractorPositions.data());

    std::vector<AttractorData> attractors(attractorCount);
    for(size_t i = 0; i < attractors.size(); ++i)
        attractors[i].position = attractorPositions[i];

    const size_t bufSize = attractorCount * sizeof(AttractorData);

    ComPtr<ID3D12Resource> attractorsData;
    device->CreateCommittedResource(
        agz::get_temp_ptr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
        D3D12_HEAP_FLAG_NONE,
        agz::get_temp_ptr(CD3DX12_RESOURCE_DESC::Buffer(bufSize)),
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(attractorsData.GetAddressOf()));

    // upload data

    uploader.uploadBufferData(
        attractorsData, attractors.data(), bufSize,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    uploader.waitForIdle();

    return attractorsData;
}

void AttractorMesh::transformToUnitCube(Triangles &triangles)
{
    Vec3 low((std::numeric_limits<float>::max)());
    Vec3 high((std::numeric_limits<float>::lowest)());

    for(auto &t : triangles)
    {
        for(int i = 0; i < 3; ++i)
        {
            low[i] = (std::min)(low[i], t.a[i]);
            low[i] = (std::min)(low[i], t.b[i]);
            low[i] = (std::min)(low[i], t.c[i]);

            high[i] = (std::max)(high[i], t.a[i]);
            high[i] = (std::max)(high[i], t.b[i]);
            high[i] = (std::max)(high[i], t.c[i]);
        }
    }

    const float maxExtent = (high - low).max_elem();
    const Vec3 offset = -0.5f * (low + high);
    const float scale = 2 / maxExtent;

    for(auto &t : triangles)
    {
        t.a = scale * (t.a + offset);
        t.b = scale * (t.b + offset);
        t.c = scale * (t.c + offset);
    }
}

void AttractorMesh::setTriangles(Triangles triangles)
{
    triangles_.swap(triangles);
    transformToUnitCube(triangles_);

    std::vector<float> areas(triangles_.size());
    for(size_t i = 0; i < triangles_.size(); ++i)
    {
        const auto &t = triangles_[i];
        areas[i] = cross(t.b - t.a, t.c - t.a).length() / 2;
    }

    triangleSampler_.initialize(areas.data(), areas.size());
}

ParticleSystem::ParticleSystem(
    ComPtr<ID3D12Device> device,
    ResourceUploader    &uploader,
    int                  frameCount,
    int                  particleCnt)
    : device_(std::move(device)),
      framegraph_(nullptr)
{
    currentFrameIndex_ = 0;

    particleCount_  = particleCnt;
    attractedCount_ = (std::max<uint32_t>)(particleCnt * 3 / 4, 1000);
    attractorCount_ = (std::max<uint32_t>)(attractedCount_ / 2, 1000);
    particleSize_   = 0.01f;

    simulationT_ = colorT_ = 0;

    // create particle buffer

    const size_t bufByteSize = sizeof(ParticleData) * particleCnt;
    const fg::BufDesc bufDesc(
        bufByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    AGZ_D3D12_CHECK_HR(
        device_->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc.desc,
            {}, nullptr,
            IID_PPV_ARGS(dataA_.GetAddressOf())));

    AGZ_D3D12_CHECK_HR(
        device_->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc.desc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
            IID_PPV_ARGS(dataB_.GetAddressOf())));

    // initialize particle data

    std::default_random_engine rng{ std::random_device()() };
    const std::uniform_real_distribution<float> dis(0, 1);

    std::vector<ParticleData> initParticles(particleCnt);
    for(auto &p : initParticles)
    {
        using agz::math::distribution::uniform_on_sphere;

        const float u1 = dis(rng);
        const float u2 = dis(rng);

        const Vec3 posOnUnitSphere = uniform_on_sphere(u1, u2).first;

        p.position = agz::math::lerp(3.5f, 5.0f, dis(rng)) * posOnUnitSphere;
        p.velocity = Vec3(0);
    }

    uploader.uploadBufferData(
        dataA_, initParticles.data(), bufByteSize,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // simulation pipeline

    simRootSignature_ = fg::RootSignature{
        {},
        fg::ConstantBufferView { D3D12_SHADER_VISIBILITY_ALL, fg::s0b0 },
        fg::DescriptorTable{
            D3D12_SHADER_VISIBILITY_ALL,
            fg::SRVRange{ fg::s0t0 },
            fg::UAVRange{ fg::s0u0 },
            fg::SRVRange{ fg::s0t1 }
        }
    }.createRootSignature(device_.Get());

    simPipeline_ = fg::ComputePipeline{
        fg::ComputeShader::loadFromFile("./asset/09_sim.hlsl", "cs_5_0"),
        simRootSignature_
    }.createPipelineState(device_.Get());

    simConsts_.initializeDynamic(device_.Get(), frameCount);

    simulationT_ = 0;

    // rendering pipeline

    rdrRootSignature_ = fg::RootSignature{
        {},
        fg::ConstantBufferView{ D3D12_SHADER_VISIBILITY_GEOMETRY, fg::s0b0 },
        fg::ConstantBufferView{ D3D12_SHADER_VISIBILITY_PIXEL,    fg::s0b1 },
        fg::DescriptorTable
        {
            D3D12_SHADER_VISIBILITY_VERTEX,
            fg::SRVRange{ fg::s0t0 }
        }
    }.createRootSignature(device_.Get());

    const auto rdrVertexShaderSource =
        agz::file::read_txt_file("./asset/09_vtx.hlsl");
    const auto rdrGeometryShaderSource =
        agz::file::read_txt_file("./asset/09_geo.hlsl");
    const auto rdrPixelShaderSource =
        agz::file::read_txt_file("./asset/09_pxl.hlsl");

    rdrPipeline_ = fg::GraphicsPipelineState{
        fg::VertexShader::loadFromFile("./asset/09_vtx.hlsl", "vs_5_0"),
        fg::GeometryShader::loadFromFile("./asset/09_geo.hlsl", "gs_5_0"),
        fg::PixelShader::loadFromFile("./asset/09_pxl.hlsl", "ps_5_0"),
        rdrRootSignature_,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
        fg::PipelineRTVFormats{ DXGI_FORMAT_R8G8B8A8_UNORM },
        fg::DepthStencilState{ DXGI_FORMAT_D24_UNORM_S8_UINT }
    }.createPipelineState(device_.Get());

    rdrGeometryConsts_.initializeDynamic(device_.Get(), frameCount);
    rdrPixelConsts_.initializeDynamic(device_.Get(), frameCount);

    colorT_ = 0;
}

void ParticleSystem::setMesh(
    ComPtr<ID3D12Resource> attractorData, uint32_t attractorCount)
{
    attractors_.Swap(attractorData);
    attractorCount_ = attractorCount;
}

void ParticleSystem::setAttractedCount(uint32_t attractedCount)
{
    attractedCount_ = attractedCount;
}

void ParticleSystem::setAttractorWorld(const Mat4 &world) noexcept
{
    attractorWorld_ = world;
}

void ParticleSystem::setParticleSize(float size)
{
    particleSize_ = size;
}

void ParticleSystem::setConfig(const Config &config) noexcept
{
    config_ = config;
}

void ParticleSystem::update(
    int frameIndex, float deltaT,
    const Mat4 &viewProj, const Vec3 &eye)
{
    currentFrameIndex_ = frameIndex;

    simulationT_ += deltaT;
    colorT_      += deltaT;

    SimulationConstants simData;
    simData.attractedParticleCount = attractedCount_;
    simData.attractorCount         = attractorCount_;
    simData.bgForceFieldT          = simulationT_;
    simData.dT                     = deltaT;
    simData.attractorWorld         = attractorWorld_;
    simData.maxVel                 = config_.maxVel;
    simData.attractorForce         = config_.attractorForce;
    simData.attractedRandomForce   = config_.attractedRandomForce;
    simData.unattractedRandomForce = config_.unattractedRandomForce;
    simData.randomForceSFreq       = config_.randomForceSFreq;
    simData.randomForceTFreq       = config_.randomForceTFreq;

    simConsts_.updateContentData(frameIndex, simData);

    RenderingGeometryConsts geoData;
    geoData.eyePos   = eye;
    geoData.geoSize  = particleSize_;
    geoData.viewProj = viewProj;

    rdrGeometryConsts_.updateContentData(frameIndex, geoData);

    RenderingPixelConsts pxlData;
    pxlData.colorT     = colorT_;
    pxlData.colorSFreq = config_.colorSFreq;
    pxlData.colorTFreq = config_.colorTFreq;

    rdrPixelConsts_.updateContentData(frameIndex, pxlData);

    framegraph_->setExternalRsc(prevData_, dataA_);
    framegraph_->setExternalRsc(nextData_, dataB_);
    framegraph_->setExternalRsc(attractorsRsc_, attractors_);
    dataA_.Swap(dataB_);
}

void ParticleSystem::initPasses(
    int               width,
    int               height,
    fg::FrameGraph   &graph,
    fg::ResourceIndex renderTarget)
{
    using namespace fg;

    framegraph_ = &graph;

    prevData_ = graph.addExternalResource(
        dataA_,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    nextData_ = graph.addExternalResource(
        dataB_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    attractorsRsc_ = graph.addExternalResource(
        attractors_,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    auto depthStencilIdx = graph.addInternalResource(
        Tex2DDesc{
            DXGI_FORMAT_D24_UNORM_S8_UINT,
            static_cast<UINT>(width),
            static_cast<UINT>(height) });

    graph.addComputePass(
        [&](ID3D12GraphicsCommandList *cmdList,
            FrameGraphPassContext &ctx)
    {
        auto prevDataIns = ctx.getResource(prevData_);

        cmdList->SetComputeRootConstantBufferView(
            0, simConsts_.getGpuVirtualAddress(currentFrameIndex_));
        cmdList->SetComputeRootDescriptorTable(
            1, prevDataIns.descriptor);

        cmdList->Dispatch(particleCount_, 1, 1);
    },
        BufSRV{ prevData_, sizeof(ParticleData), particleCount_ },
        BufUAV{ nextData_, sizeof(ParticleData), particleCount_ },
        BufSRV{ attractorsRsc_, sizeof(AttractorMesh::AttractorData), attractorCount_ },
        simPipeline_,
        simRootSignature_);

    graph.addGraphicsPass(
        [&](ID3D12GraphicsCommandList *cmdList,
            FrameGraphPassContext &ctx)
    {
        auto prevDataIns = ctx.getResource(prevData_);

        cmdList->SetGraphicsRootConstantBufferView(
            0, rdrGeometryConsts_.getGpuVirtualAddress(currentFrameIndex_));
        cmdList->SetGraphicsRootConstantBufferView(
            1, rdrPixelConsts_.getGpuVirtualAddress(currentFrameIndex_));
        cmdList->SetGraphicsRootDescriptorTable(
            2, prevDataIns.descriptor);

        cmdList->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

        cmdList->DrawInstanced(particleCount_, 1, 0, 0);
    },
        BufSRV{ prevData_, sizeof(ParticleData), particleCount_ },
        RenderTargetBinding{ Tex2DRTV{ renderTarget }, ClearColor{} },
        DepthStencilBinding{ Tex2DDSV{ depthStencilIdx }, ClearDepthStencil{} },
        rdrPipeline_,
        rdrRootSignature_);
}

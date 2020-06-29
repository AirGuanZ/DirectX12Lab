#include <agz/utility/image.h>
#include <agz/utility/mesh.h>

#include "./mesh.h"

void Mesh::loadFromFile(
    const Window      &window,
    ResourceUploader  &uploader,
    DescriptorSubHeap &descHeap,
    const std::string &objFilename,
    const std::string &albedoFilename)
{
    albedoDescTable_ = descHeap.allocRange(1);

    std::vector<ComPtr<ID3D12Resource>> ret;

    // albedo texture

    const auto imgData = agz::texture::texture2d_t<agz::math::color4b>(
        agz::img::load_rgba_from_file(albedoFilename));
    if(!imgData.is_available())
    {
        throw std::runtime_error(
            "failed to load image data from " + albedoFilename);
    }

    albedo_.initialize(
        window.getDevice(),
        DXGI_FORMAT_R8G8B8A8_UNORM,
        imgData.width(), imgData.height(),
        1, 1, 1, 0, {}, {});

    uploader.uploadTex2DData(
        albedo_, ResourceUploader::Tex2DSubInitData{ imgData.raw_data() },
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    albedo_.createSRV(albedoDescTable_[0]);
    
    // constant buffer
    
    vsTransform_.initializeDynamic(window.getDevice(), window.getImageCount());

    // vertex data

    const auto mesh =
        triangle_to_vertex(agz::mesh::load_from_file(objFilename));

    std::vector<Vertex> vertexData;
    std::transform(
        mesh.begin(), mesh.end(), std::back_inserter(vertexData),
        [](const agz::mesh::vertex_t &v)
    {
        return Vertex{ v.position, v.normal, v.tex_coord };
    });

    vertexBuffer_.initializeDefault(window.getDevice(), vertexData.size(), {});

    uploader.uploadBufferData(
        vertexBuffer_, vertexData.data(),
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

void Mesh::setWorldTransform(const Mat4 &world) noexcept
{
    world_ = world;
}

void Mesh::draw(
    ID3D12GraphicsCommandList *cmdList,
    int                        imageIndex,
    const WalkingCamera       &camera) const
{
    const Mat4 wvp = world_ * camera.getViewProj();
    vsTransform_.updateContentData(imageIndex, { wvp, world_ });

    cmdList->SetGraphicsRootConstantBufferView(
        0, vsTransform_.getGpuVirtualAddress(imageIndex));

    cmdList->SetGraphicsRootDescriptorTable(
        1, albedoDescTable_[0].getGPUHandle());

    const auto vertexBufferView = vertexBuffer_.getView();
    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

    cmdList->DrawInstanced(UINT(vertexBuffer_.getVertexCount()), 1, 0, 0);
}

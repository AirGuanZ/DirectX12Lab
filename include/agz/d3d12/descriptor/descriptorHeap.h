#pragma once

#include <set>

#include <agz/d3d12/descriptor/rawDescriptorHeap.h>
#include <agz/utility/container.h>

AGZ_D3D12_BEGIN

using DescriptorIndex = uint32_t;

using DescriptorCount = std::make_unsigned_t<DescriptorIndex>;

class Descriptor
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_;

public:

    Descriptor() noexcept;

    Descriptor(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) noexcept;

    const D3D12_CPU_DESCRIPTOR_HANDLE &getCPUHandle() const noexcept;

    const D3D12_GPU_DESCRIPTOR_HANDLE &getGPUHandle() const noexcept;

    operator D3D12_CPU_DESCRIPTOR_HANDLE() const noexcept;

    operator D3D12_GPU_DESCRIPTOR_HANDLE() const noexcept;
};

class DescriptorRange
{
    friend class DescriptorSubHeap;

    RawDescriptorHeap *rawHeap_;

    DescriptorIndex beg_;
    DescriptorCount cnt_;

public:

    DescriptorRange() noexcept;

    DescriptorRange(
        RawDescriptorHeap *rawHeap,
        DescriptorIndex beg,
        DescriptorCount cnt) noexcept;

    DescriptorRange getSubRange(
        DescriptorIndex start, DescriptorCount size) const noexcept;

    Descriptor operator[](DescriptorIndex idx) const noexcept;

    DescriptorCount getCount() const noexcept;

    DescriptorIndex getStartIndexInRawHeap() const noexcept;
};

class DescriptorSubHeap : public misc::uncopyable_t
{
    friend class DescriptorHeap;

    RawDescriptorHeap *rawHeap_;

    container::interval_mgr_t<uint32_t> freeBlocks_;

    DescriptorIndex beg_;
    DescriptorIndex end_;

    void destroy();

public:

    DescriptorSubHeap();

    DescriptorSubHeap(
        DescriptorSubHeap &&other) noexcept;

    DescriptorSubHeap &operator=(
        DescriptorSubHeap &&other) noexcept;

    ~DescriptorSubHeap();

    void swap(DescriptorSubHeap &other) noexcept;

    void initialize(
        RawDescriptorHeap *rawHeap,
        DescriptorIndex    beg,
        DescriptorIndex    end);

    bool isAvailable() const noexcept;

    ID3D12DescriptorHeap *getRawHeap() const noexcept;

    std::optional<DescriptorSubHeap> allocSubHeap(
        DescriptorCount subHeapSize);

    std::optional<DescriptorRange> allocRange(
        DescriptorCount count);

    std::optional<Descriptor> allocSingle();

    void freeAll();

    void freeSubHeap(DescriptorSubHeap &&subheap);

    void freeRange(const DescriptorRange &range);

    void freeSingle(Descriptor descriptor);
};

class DescriptorHeap : DescriptorSubHeap
{
    RawDescriptorHeap rawHeap_;

public:

    DescriptorHeap();

    void initialize(
        ID3D12Device              *device,
        DescriptorCount            size,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        bool                       shaderVisible);

    bool isAvailable() const noexcept;

    void destroy();

    using DescriptorSubHeap::getRawHeap;

    using DescriptorSubHeap::allocSubHeap;
    using DescriptorSubHeap::allocRange;
    using DescriptorSubHeap::allocSingle;

    using DescriptorSubHeap::freeSubHeap;
    using DescriptorSubHeap::freeRange;
    using DescriptorSubHeap::freeSingle;

    DescriptorSubHeap       &getRootSubheap() noexcept;
    const DescriptorSubHeap &getRootSubheap() const noexcept;
};

inline Descriptor::Descriptor() noexcept
    : cpuHandle_{ 0 }, gpuHandle_{ 0 }
{

}

inline Descriptor::Descriptor(
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) noexcept
    : cpuHandle_(cpuHandle), gpuHandle_(gpuHandle)
{
    
}

inline const D3D12_CPU_DESCRIPTOR_HANDLE &Descriptor::getCPUHandle() const noexcept
{
    return cpuHandle_;
}

inline const D3D12_GPU_DESCRIPTOR_HANDLE &Descriptor::getGPUHandle() const noexcept
{
    return gpuHandle_;
}

inline Descriptor::operator struct D3D12_CPU_DESCRIPTOR_HANDLE() const noexcept
{
    return getCPUHandle();
}

inline Descriptor::operator struct D3D12_GPU_DESCRIPTOR_HANDLE() const noexcept
{
    return getGPUHandle();
}

inline DescriptorRange::DescriptorRange() noexcept
    : DescriptorRange(nullptr, 0, 0)
{
    
}

inline DescriptorRange::DescriptorRange(
    RawDescriptorHeap *rawHeap,
    DescriptorIndex    beg,
    DescriptorCount    cnt) noexcept
    : rawHeap_(rawHeap), beg_(beg), cnt_(cnt)
{
    
}

inline DescriptorRange DescriptorRange::getSubRange(
    DescriptorIndex start,
    DescriptorCount size) const noexcept
{
    assert(start + size <= cnt_);
    return DescriptorRange(rawHeap_, beg_ + start, size);
}

inline Descriptor DescriptorRange::operator[](
    DescriptorIndex idx) const noexcept
{
    assert(idx < cnt_);
    const DescriptorIndex rawIdx = beg_ + idx;
    return Descriptor(
        rawHeap_->getCPUHandle(rawIdx),
        rawHeap_->getGPUHandle(rawIdx));
}

inline DescriptorCount DescriptorRange::getCount() const noexcept
{
    return cnt_;
}

inline DescriptorIndex DescriptorRange::getStartIndexInRawHeap() const noexcept
{
    return beg_;
}

inline void DescriptorSubHeap::destroy()
{
    rawHeap_ = nullptr;
    freeBlocks_ = container::interval_mgr_t<DescriptorIndex>();
}

inline DescriptorSubHeap::DescriptorSubHeap()
    : rawHeap_(nullptr), beg_(0), end_(0)
{
    
}

inline DescriptorSubHeap::DescriptorSubHeap(DescriptorSubHeap &&other) noexcept
    : DescriptorSubHeap()
{
    swap(other);
}

inline DescriptorSubHeap &DescriptorSubHeap::operator=(
    DescriptorSubHeap &&other) noexcept
{
    swap(other);
    return *this;
}

inline DescriptorSubHeap::~DescriptorSubHeap()
{
    destroy();
}

inline void DescriptorSubHeap::swap(DescriptorSubHeap &other) noexcept
{
    std::swap(rawHeap_,    other.rawHeap_);
    freeBlocks_.swap(other.freeBlocks_);
    std::swap(beg_,        other.beg_);
    std::swap(end_,        other.end_);
}

inline void DescriptorSubHeap::initialize(
    RawDescriptorHeap *rawHeap,
    DescriptorIndex    beg,
    DescriptorIndex    end)
{
    destroy();

    misc::scope_guard_t destroyGuard([&] { destroy(); });

    freeBlocks_.free(beg, end);
    rawHeap_ = rawHeap;
    beg_     = beg;
    end_     = end;

    destroyGuard.dismiss();
}

inline bool DescriptorSubHeap::isAvailable() const noexcept
{
    return rawHeap_ != nullptr;
}

inline ID3D12DescriptorHeap *DescriptorSubHeap::getRawHeap() const noexcept
{
    return rawHeap_->getHeap();
}

inline std::optional<DescriptorSubHeap> DescriptorSubHeap::allocSubHeap(
    DescriptorCount subHeapSize)
{
    const auto obeg = freeBlocks_.alloc(subHeapSize);
    if(!obeg)
        return std::nullopt;

    DescriptorSubHeap subheap;
    subheap.initialize(rawHeap_, *obeg, *obeg + subHeapSize);

    return std::make_optional(std::move(subheap));
}

inline std::optional<DescriptorRange> DescriptorSubHeap::allocRange(
    DescriptorCount count)
{
    const auto obeg = freeBlocks_.alloc(count);
    if(!obeg)
        return std::nullopt;
    return std::make_optional<DescriptorRange>(rawHeap_, *obeg, count);
}

inline std::optional<Descriptor> DescriptorSubHeap::allocSingle()
{
    auto orange = allocRange(1);
    return orange ? std::make_optional((*orange)[0]) : std::nullopt;
}

inline void DescriptorSubHeap::freeAll()
{
    initialize(rawHeap_, beg_, end_);
}

inline void DescriptorSubHeap::freeSubHeap(DescriptorSubHeap &&subheap)
{
    assert(subheap.rawHeap_ == rawHeap_);
    freeBlocks_.free(subheap.beg_, subheap.end_);
    subheap.destroy();
}

inline void DescriptorSubHeap::freeRange(const DescriptorRange &range)
{
    assert(range.rawHeap_ == rawHeap_);
    freeBlocks_.free(range.beg_, range.beg_ + range.cnt_);
}

inline void DescriptorSubHeap::freeSingle(Descriptor descriptor)
{
    const auto rawDiff = descriptor.getCPUHandle().ptr -
                         rawHeap_->getCPUHandle(0).ptr;
    assert(rawDiff % rawHeap_->getDescIncSize() == 0);
    const DescriptorIndex idx = DescriptorIndex(
        rawDiff / rawHeap_->getDescIncSize());

    freeBlocks_.free(idx, idx + 1);
}

inline DescriptorHeap::DescriptorHeap()
{
    
}

inline void DescriptorHeap::initialize(
    ID3D12Device              *device,
    DescriptorCount            size,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    bool                       shaderVisible)
{
    rawHeap_ = RawDescriptorHeap(device, size, type, shaderVisible);
    misc::scope_guard_t destroyGuard([&] { destroy(); });

    DescriptorSubHeap::initialize(&rawHeap_, 0, size);
    destroyGuard.dismiss();
}

inline bool DescriptorHeap::isAvailable() const noexcept
{
    return rawHeap_.isAvailable();
}

inline void DescriptorHeap::destroy()
{
    DescriptorSubHeap::destroy();
    rawHeap_ = RawDescriptorHeap();
}

inline DescriptorSubHeap &DescriptorHeap::getRootSubheap() noexcept
{
    return static_cast<DescriptorSubHeap &>(*this);
}

inline const DescriptorSubHeap &DescriptorHeap::getRootSubheap() const noexcept
{
    return static_cast<const DescriptorSubHeap &>(*this);
}

AGZ_D3D12_END

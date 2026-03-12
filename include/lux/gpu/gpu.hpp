// Copyright (c) 2024-2026 Lux Industries Inc.
// SPDX-License-Identifier: BSD-3-Clause
//
// lux/gpu/gpu.hpp - Base GPU Runtime
//
// Backend-agnostic GPU primitives. Backends (Metal, WebGPU, CUDA)
// implement the Backend interface and register via plugins.

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lux::gpu {

// ============================================================================
// Forward Declarations
// ============================================================================

class Context;
class Device;
class Queue;
class Buffer;
class Kernel;
class Backend;

// ============================================================================
// Status
// ============================================================================

enum class StatusCode {
    Ok = 0,
    InvalidArgument,
    OutOfMemory,
    DeviceLost,
    BackendNotAvailable,
    KernelNotFound,
    KernelCompilationFailed,
    DispatchFailed,
    Timeout,
    NotSupported,
    InternalError,
};

struct Status {
    StatusCode code = StatusCode::Ok;
    std::string message;

    [[nodiscard]] bool ok() const { return code == StatusCode::Ok; }
    [[nodiscard]] explicit operator bool() const { return ok(); }

    static Status Ok() { return {StatusCode::Ok, ""}; }
    static Status Error(StatusCode c, std::string msg = "") { return {c, std::move(msg)}; }
};

template <typename T>
using Result = std::expected<T, Status>;

// ============================================================================
// Backend Type
// ============================================================================

enum class BackendType {
    Auto,
    Metal,
    WebGPU,
    CUDA,
};

[[nodiscard]] constexpr std::string_view backendName(BackendType t) {
    switch (t) {
        case BackendType::Metal: return "metal";
        case BackendType::WebGPU: return "webgpu";
        case BackendType::CUDA: return "cuda";
        default: return "auto";
    }
}

// ============================================================================
// Device Info
// ============================================================================

struct DeviceFeatures {
    bool fp16 = false;
    bool fp64 = false;
    bool int64_atomics = false;
    bool subgroups = false;
    uint32_t max_workgroup_size = 256;
    uint32_t max_workgroups[3] = {65535, 65535, 65535};
    size_t max_buffer_size = 0;
    size_t max_shared_memory = 0;
    uint32_t simd_width = 32;
};

struct DeviceInfo {
    std::string name;
    std::string vendor;
    BackendType backend = BackendType::Auto;
    DeviceFeatures features;
    size_t total_memory = 0;
    bool is_discrete = false;
    bool is_unified_memory = false;
};

// ============================================================================
// Buffer
// ============================================================================

enum class BufferUsage : uint32_t {
    Storage = 1 << 0,
    Uniform = 1 << 1,
    CopySrc = 1 << 4,
    CopyDst = 1 << 5,
    MapRead = 1 << 6,
    MapWrite = 1 << 7,
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(BufferUsage a, BufferUsage b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

class Buffer {
public:
    virtual ~Buffer() = default;
    [[nodiscard]] virtual size_t size() const = 0;
    [[nodiscard]] virtual BufferUsage usage() const = 0;
    [[nodiscard]] virtual BackendType backend() const = 0;
    [[nodiscard]] virtual Result<void*> map() = 0;
    virtual void unmap() = 0;
    [[nodiscard]] virtual void* nativeHandle() const = 0;
};

using BufferPtr = std::shared_ptr<Buffer>;

struct BufferView {
    BufferPtr buffer;
    size_t offset = 0;
    size_t length = 0;
    [[nodiscard]] size_t effectiveSize() const {
        return length > 0 ? length : (buffer ? buffer->size() - offset : 0);
    }
};

// ============================================================================
// Kernel
// ============================================================================

struct DispatchSize { uint32_t x = 1, y = 1, z = 1; };

class Kernel {
public:
    virtual ~Kernel() = default;
    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual BackendType backend() const = 0;
    virtual void setWorkgroupSize(uint32_t x, uint32_t y = 1, uint32_t z = 1) = 0;
    virtual void setArg(uint32_t index, const BufferView& buf) = 0;
    virtual void setArg(uint32_t index, const void* data, size_t size) = 0;

    template <typename T>
    void setArg(uint32_t index, const T& value) { setArg(index, &value, sizeof(T)); }
};

using KernelPtr = std::shared_ptr<Kernel>;

class KernelHandle {
public:
    KernelHandle() = default;
    explicit KernelHandle(uint64_t id) : id_(id) {}
    [[nodiscard]] uint64_t id() const { return id_; }
    [[nodiscard]] bool valid() const { return id_ != 0; }
private:
    uint64_t id_ = 0;
};

// ============================================================================
// Queue
// ============================================================================

class Queue {
public:
    virtual ~Queue() = default;
    [[nodiscard]] virtual BackendType backend() const = 0;
    virtual Status dispatch(Kernel& kernel, DispatchSize grid) = 0;
    virtual Status copy(const BufferView& src, const BufferView& dst) = 0;
    virtual Status fill(const BufferView& dst, uint8_t value) = 0;
    virtual Status submit() = 0;
    virtual Status waitIdle() = 0;
    virtual void onComplete(std::function<void(Status)> callback) = 0;
};

using QueuePtr = std::shared_ptr<Queue>;

// ============================================================================
// Device
// ============================================================================

class Device {
public:
    virtual ~Device() = default;
    [[nodiscard]] virtual const DeviceInfo& info() const = 0;
    [[nodiscard]] virtual BackendType backend() const = 0;
    [[nodiscard]] virtual Result<BufferPtr> createBuffer(size_t size, BufferUsage usage) = 0;
    [[nodiscard]] virtual Result<BufferPtr> createBuffer(std::span<const uint8_t> data, BufferUsage usage) = 0;
    [[nodiscard]] virtual Result<QueuePtr> createQueue() = 0;
    [[nodiscard]] virtual Result<KernelPtr> createKernel(std::string_view source, std::string_view entry) = 0;
    [[nodiscard]] virtual Result<KernelPtr> getKernel(KernelHandle handle) = 0;
};

using DevicePtr = std::shared_ptr<Device>;

// ============================================================================
// Backend (implemented by lux-metal, lux-webgpu, lux-cuda)
// ============================================================================

class Backend {
public:
    virtual ~Backend() = default;
    [[nodiscard]] virtual BackendType type() const = 0;
    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual std::vector<DeviceInfo> enumerateDevices() const = 0;
    [[nodiscard]] virtual Result<DevicePtr> createDevice(size_t index = 0) = 0;
};

using BackendPtr = std::shared_ptr<Backend>;
using BackendCreateFn = BackendPtr(*)();

// ============================================================================
// Kernel Registry
// ============================================================================

struct KernelBundleInfo {
    std::string name;
    std::string version;
    BackendType backend;
    std::vector<std::string> ops;
};

class KernelRegistry {
public:
    static KernelRegistry& instance();

    Status loadBundle(std::string_view path);
    Status loadBundleFromMemory(std::span<const uint8_t> data, BackendType backend);
    [[nodiscard]] std::vector<KernelBundleInfo> loadedBundles() const;
    [[nodiscard]] KernelHandle find(std::string_view op, BackendType backend = BackendType::Auto) const;
    [[nodiscard]] std::string_view getSource(KernelHandle handle) const;
    [[nodiscard]] std::string_view getEntryPoint(KernelHandle handle) const;
    [[nodiscard]] std::vector<std::string_view> listOps(BackendType backend = BackendType::Auto) const;

private:
    KernelRegistry();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Context
// ============================================================================

class Context {
public:
    Context();
    ~Context();

    Status loadBackend(std::string_view path);
    Status loadBackend(BackendPtr backend);
    [[nodiscard]] std::vector<BackendType> availableBackends() const;
    [[nodiscard]] BackendPtr getBackend(BackendType type) const;
    [[nodiscard]] std::vector<DeviceInfo> enumerateDevices(BackendType type = BackendType::Auto) const;
    [[nodiscard]] Result<DevicePtr> createDevice(BackendType type = BackendType::Auto, size_t index = 0);
    [[nodiscard]] DevicePtr defaultDevice();
    [[nodiscard]] KernelRegistry& kernels();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Global context
Context& context();
inline DevicePtr defaultDevice() { return context().defaultDevice(); }
inline KernelHandle findKernel(std::string_view op, BackendType backend = BackendType::Auto) {
    return context().kernels().find(op, backend);
}

} // namespace lux::gpu

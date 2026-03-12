// Copyright (c) 2024-2026 Lux Industries Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <lux/gpu/gpu.hpp>
#include <dlfcn.h>
#include <mutex>
#include <unordered_map>

namespace lux::gpu {

struct Context::Impl {
    std::vector<BackendPtr> backends;
    std::unordered_map<BackendType, BackendPtr> backend_map;
    DevicePtr default_device;
    std::mutex mutex;

    BackendPtr selectBackend(BackendType type) {
        if (type != BackendType::Auto) {
            auto it = backend_map.find(type);
            return it != backend_map.end() ? it->second : nullptr;
        }
        static const BackendType priority[] = {BackendType::Metal, BackendType::CUDA, BackendType::WebGPU};
        for (auto bt : priority) {
            auto it = backend_map.find(bt);
            if (it != backend_map.end() && it->second->isAvailable()) return it->second;
        }
        return nullptr;
    }
};

Context::Context() : impl_(std::make_unique<Impl>()) {}
Context::~Context() = default;

Status Context::loadBackend(std::string_view path) {
    void* handle = dlopen(std::string(path).c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) return Status::Error(StatusCode::BackendNotAvailable, dlerror());

    auto fn = reinterpret_cast<BackendCreateFn>(dlsym(handle, "lux_gpu_create_backend"));
    if (!fn) { dlclose(handle); return Status::Error(StatusCode::BackendNotAvailable, "Missing symbol"); }

    auto backend = fn();
    if (!backend) { dlclose(handle); return Status::Error(StatusCode::BackendNotAvailable, "Create failed"); }

    return loadBackend(std::move(backend));
}

Status Context::loadBackend(BackendPtr backend) {
    std::lock_guard lock(impl_->mutex);
    if (!backend->isAvailable()) return Status::Error(StatusCode::BackendNotAvailable);
    impl_->backends.push_back(backend);
    impl_->backend_map[backend->type()] = backend;
    return Status::Ok();
}

std::vector<BackendType> Context::availableBackends() const {
    std::lock_guard lock(impl_->mutex);
    std::vector<BackendType> result;
    for (const auto& [type, backend] : impl_->backend_map)
        if (backend->isAvailable()) result.push_back(type);
    return result;
}

BackendPtr Context::getBackend(BackendType type) const {
    std::lock_guard lock(impl_->mutex);
    auto it = impl_->backend_map.find(type);
    return it != impl_->backend_map.end() ? it->second : nullptr;
}

std::vector<DeviceInfo> Context::enumerateDevices(BackendType type) const {
    std::lock_guard lock(impl_->mutex);
    std::vector<DeviceInfo> result;
    if (type == BackendType::Auto) {
        for (const auto& [_, backend] : impl_->backend_map) {
            auto devices = backend->enumerateDevices();
            result.insert(result.end(), devices.begin(), devices.end());
        }
    } else {
        auto it = impl_->backend_map.find(type);
        if (it != impl_->backend_map.end()) result = it->second->enumerateDevices();
    }
    return result;
}

Result<DevicePtr> Context::createDevice(BackendType type, size_t index) {
    std::lock_guard lock(impl_->mutex);
    auto backend = impl_->selectBackend(type);
    if (!backend) return std::unexpected(Status::Error(StatusCode::BackendNotAvailable));
    return backend->createDevice(index);
}

DevicePtr Context::defaultDevice() {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->default_device) {
        auto backend = impl_->selectBackend(BackendType::Auto);
        if (backend) {
            auto result = backend->createDevice(0);
            if (result) impl_->default_device = *result;
        }
    }
    return impl_->default_device;
}

KernelRegistry& Context::kernels() { return KernelRegistry::instance(); }

namespace {
    std::unique_ptr<Context> g_context;
    std::once_flag g_init;
}

Context& context() {
    std::call_once(g_init, []() { g_context = std::make_unique<Context>(); });
    return *g_context;
}

} // namespace lux::gpu

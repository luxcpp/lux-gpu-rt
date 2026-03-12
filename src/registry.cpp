// Copyright (c) 2024-2026 Lux Industries Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <lux/gpu/gpu.hpp>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace lux::gpu {

struct KernelEntry {
    std::string op;
    BackendType backend;
    std::string source;
    std::string entry_point;
};

struct KernelRegistry::Impl {
    std::vector<KernelBundleInfo> bundles;
    std::unordered_map<uint64_t, KernelEntry> kernels;
    std::unordered_map<std::string, std::vector<uint64_t>> op_to_kernels;
    uint64_t next_id = 1;
    mutable std::mutex mutex;

    uint64_t addKernel(const KernelEntry& entry) {
        uint64_t id = next_id++;
        kernels[id] = entry;
        op_to_kernels[entry.op].push_back(id);
        return id;
    }
};

KernelRegistry::KernelRegistry() : impl_(std::make_unique<Impl>()) {}

KernelRegistry& KernelRegistry::instance() {
    static KernelRegistry registry;
    return registry;
}

Status KernelRegistry::loadBundle(std::string_view path) {
    (void)path;
    return Status::Error(StatusCode::NotSupported, "Bundle loading not implemented");
}

Status KernelRegistry::loadBundleFromMemory(std::span<const uint8_t> data, BackendType backend) {
    std::lock_guard lock(impl_->mutex);
    if (data.size() < 16) return Status::Error(StatusCode::InvalidArgument, "Bundle too small");

    const uint8_t* ptr = data.data();
    if (std::memcmp(ptr, "LKRN", 4) != 0)
        return Status::Error(StatusCode::InvalidArgument, "Invalid magic");

    ptr += 4;
    ptr += 4; // version
    uint32_t num_kernels = *reinterpret_cast<const uint32_t*>(ptr); ptr += 4;
    uint32_t name_len = *reinterpret_cast<const uint32_t*>(ptr); ptr += 4;
    std::string bundle_name(reinterpret_cast<const char*>(ptr), name_len); ptr += name_len;

    KernelBundleInfo info{bundle_name, "1.0.0", backend, {}};

    for (uint32_t i = 0; i < num_kernels; ++i) {
        KernelEntry entry; entry.backend = backend;
        uint32_t op_len = *reinterpret_cast<const uint32_t*>(ptr); ptr += 4;
        entry.op = std::string(reinterpret_cast<const char*>(ptr), op_len); ptr += op_len;
        uint32_t entry_len = *reinterpret_cast<const uint32_t*>(ptr); ptr += 4;
        entry.entry_point = std::string(reinterpret_cast<const char*>(ptr), entry_len); ptr += entry_len;
        uint32_t source_len = *reinterpret_cast<const uint32_t*>(ptr); ptr += 4;
        entry.source = std::string(reinterpret_cast<const char*>(ptr), source_len); ptr += source_len;
        impl_->addKernel(entry);
        info.ops.push_back(entry.op);
    }

    impl_->bundles.push_back(std::move(info));
    return Status::Ok();
}

std::vector<KernelBundleInfo> KernelRegistry::loadedBundles() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->bundles;
}

KernelHandle KernelRegistry::find(std::string_view op, BackendType backend) const {
    std::lock_guard lock(impl_->mutex);
    auto it = impl_->op_to_kernels.find(std::string(op));
    if (it == impl_->op_to_kernels.end()) return {};
    for (uint64_t id : it->second) {
        const auto& entry = impl_->kernels.at(id);
        if (backend == BackendType::Auto || entry.backend == backend) return KernelHandle{id};
    }
    return {};
}

std::string_view KernelRegistry::getSource(KernelHandle handle) const {
    std::lock_guard lock(impl_->mutex);
    auto it = impl_->kernels.find(handle.id());
    return it != impl_->kernels.end() ? it->second.source : std::string_view{};
}

std::string_view KernelRegistry::getEntryPoint(KernelHandle handle) const {
    std::lock_guard lock(impl_->mutex);
    auto it = impl_->kernels.find(handle.id());
    return it != impl_->kernels.end() ? it->second.entry_point : std::string_view{};
}

std::vector<std::string_view> KernelRegistry::listOps(BackendType backend) const {
    std::lock_guard lock(impl_->mutex);
    std::vector<std::string_view> result;
    for (const auto& [op, ids] : impl_->op_to_kernels) {
        for (uint64_t id : ids) {
            const auto& entry = impl_->kernels.at(id);
            if (backend == BackendType::Auto || entry.backend == backend) {
                result.push_back(op);
                break;
            }
        }
    }
    return result;
}

} // namespace lux::gpu

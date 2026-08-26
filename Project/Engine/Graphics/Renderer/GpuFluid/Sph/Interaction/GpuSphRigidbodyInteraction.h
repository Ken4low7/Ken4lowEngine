#pragma once

#include "../Manager/GpuSphManager.h"
#include "../Resource/GpuSphParticleBuffer.h"

#include <ActorHandle.h>
#include <ActorWorld.h>
#include <ColliderComponent.h>
#include <DirectXCommon.h>
#include <ResourceManager.h>
#include <RigidbodyComponent.h>
#include <ShaderCompiler.h>
#include <ShaderManifestTypes.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{

struct GpuSphRigidbodyInteractionSettings
{
    bool enabled = true;
    float particleRadiusScale = 0.5f;
    float minimumParticleRadius = 0.02f;
    float restitution = 0.05f;
    float friction = 0.15f;
    float couplingStrength = 1.0f;
    float maximumLinearImpulse = 50.0f;
    float maximumAngularImpulse = 50.0f;
};

struct GpuSphRigidbodyInteractionStats
{
    uint64_t collisionDispatchCount = 0;
    uint64_t reactionClearDispatchCount = 0;
    uint64_t readbackCount = 0;
    uint64_t appliedBodyCount = 0;
    uint32_t proxyCount = 0;
    uint32_t dynamicBodyCount = 0;
    uint32_t frameResourceCount = 0;
    float particleRadius = 0.0f;
    float lastLinearImpulse = 0.0f;
    float lastAngularImpulse = 0.0f;
    bool initialized = false;
    bool lastDispatchSucceeded = true;
};

/// W9: GPU SPH粒子とActorWorldのRigidbody/Colliderを双方向に接続するRuntime。
class GpuSphRigidbodyInteraction final
{
public:
    static constexpr uint32_t kMaxProxies = 64;
    static constexpr uint32_t kMaxDynamicBodies = 32;

    static GpuSphRigidbodyInteraction* GetInstance()
    {
        static GpuSphRigidbodyInteraction instance;
        return &instance;
    }

    void Update(ActorWorld& actorWorld)
    {
        GpuSphManager* sphManager = GpuSphManager::GetInstance();
        if (!settings_.enabled || !sphManager || !sphManager->IsInitialized())
        {
            return;
        }

        GpuSphParticleBuffer& particleBuffer = sphManager->GetParticleBuffer();
        if (!particleBuffer.IsInitialized() || particleBuffer.GetActiveParticleCount() == 0)
        {
            return;
        }

        if (!EnsureInitialized())
        {
            stats_.lastDispatchSucceeded = false;
            return;
        }

        DX12CommandManager* commandManager = dxCommon_->GetCommandManager();
        ID3D12GraphicsCommandList* commandList = commandManager ? commandManager->GetCommandList() : nullptr;
        if (!commandManager || !commandList || frameSlots_.empty())
        {
            stats_.lastDispatchSucceeded = false;
            return;
        }

        const uint32_t frameIndex = commandManager->GetCurrentFrameIndex();
        if (frameIndex >= frameSlots_.size())
        {
            Finalize();
            if (!EnsureInitialized())
            {
                stats_.lastDispatchSucceeded = false;
                return;
            }
        }

        FrameSlot& slot = frameSlots_[commandManager->GetCurrentFrameIndex()];
        ConsumeCompletedReadback(actorWorld, slot);
        GatherRigidbodyProxies(actorWorld, slot);

        stats_.proxyCount = slot.proxyCount;
        stats_.dynamicBodyCount = slot.bodyCount;
        stats_.particleRadius = CalculateParticleRadius(sphManager->GetSimulationSettings());

        if (slot.proxyCount == 0)
        {
            stats_.lastDispatchSucceeded = true;
            return;
        }

        TransitionReactionBuffer(commandList, slot, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        InteractionConstants constants{};
        constants.activeParticleCount = particleBuffer.GetActiveParticleCount();
        constants.proxyCount = slot.proxyCount;
        constants.bodyCount = slot.bodyCount;
        constants.accumulateReaction = slot.bodyCount > 0 ? 1u : 0u;
        constants.particleMass = (std::max)(sphManager->GetSimulationSettings().particleMass, 1.0e-6f);
        constants.particleRadius = stats_.particleRadius;
        constants.couplingStrength = (std::max)(settings_.couplingStrength, 0.0f);
        constants.collisionRestitution = (std::clamp)(settings_.restitution, 0.0f, 1.0f);
        constants.collisionFriction = (std::clamp)(settings_.friction, 0.0f, 1.0f);
        constants.deltaTime = (std::max)(sphManager->GetSimulationSettings().fixedDeltaTime, 1.0e-6f);
        constants.impulseScale = kImpulseFixedPointScale;

        const FrameUploadArena::Allocation allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
        if (!allocation.IsValid())
        {
            stats_.lastDispatchSucceeded = false;
            return;
        }

        particleBuffer.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        if (slot.bodyCount > 0)
        {
            BindCommonResources(commandList, slot, particleBuffer, allocation.gpuAddress, clearPipelineState_.Get());
            commandList->Dispatch((slot.bodyCount + 63u) / 64u, 1, 1);
            InsertUavBarrier(commandList, slot.reactionBuffer.Get());
            ++stats_.reactionClearDispatchCount;
        }

        BindCommonResources(commandList, slot, particleBuffer, allocation.gpuAddress, collisionPipelineState_.Get());
        commandList->Dispatch((constants.activeParticleCount + 127u) / 128u, 1, 1);
        particleBuffer.InsertUavBarrier(commandList);
        InsertUavBarrier(commandList, slot.reactionBuffer.Get());
        ++stats_.collisionDispatchCount;

        if (slot.bodyCount > 0)
        {
            TransitionReactionBuffer(commandList, slot, D3D12_RESOURCE_STATE_COPY_SOURCE);
            const uint64_t copyBytes = static_cast<uint64_t>(slot.bodyCount) * sizeof(GpuReaction);
            commandList->CopyBufferRegion(slot.readbackBuffer.Get(), 0, slot.reactionBuffer.Get(), 0, copyBytes);
            slot.pendingReadback = true;
            slot.pendingBodyCount = slot.bodyCount;
        }

        stats_.lastDispatchSucceeded = true;
    }

    void Finalize()
    {
        for (FrameSlot& slot : frameSlots_)
        {
            if (slot.proxyUploadBuffer && slot.mappedProxyData)
            {
                slot.proxyUploadBuffer->Unmap(0, nullptr);
            }
            slot.mappedProxyData = nullptr;
            slot.proxyUploadBuffer.Reset();
            slot.reactionBuffer.Reset();
            slot.readbackBuffer.Reset();
            slot.pendingReadback = false;
            slot.bodyCount = 0;
            slot.proxyCount = 0;
        }
        frameSlots_.clear();
        clearPipelineState_.Reset();
        collisionPipelineState_.Reset();
        rootSignature_.Reset();
        dxCommon_ = nullptr;
        initialized_ = false;
        stats_.initialized = false;
    }

    [[nodiscard]] GpuSphRigidbodyInteractionSettings& GetEditableSettings() { return settings_; }
    [[nodiscard]] const GpuSphRigidbodyInteractionSettings& GetSettings() const { return settings_; }
    [[nodiscard]] const GpuSphRigidbodyInteractionStats& GetStats() const { return stats_; }
    [[nodiscard]] bool IsInitialized() const { return initialized_; }

private:
    enum class ProxyShape : uint32_t
    {
        Sphere = 0,
        Aabb = 1,
        Obb = 2,
        Capsule = 3,
    };

    struct GpuProxy
    {
        uint32_t shapeType = 0;
        uint32_t bodyIndex = UINT32_MAX;
        float restitution = 0.0f;
        float friction = 0.0f;

        Vector3 center{};
        float radius = 0.0f;
        Vector3 halfSize{};
        float capsuleHalfLength = 0.0f;

        Vector3 axisX{ 1.0f, 0.0f, 0.0f };
        float padding0 = 0.0f;
        Vector3 axisY{ 0.0f, 1.0f, 0.0f };
        float padding1 = 0.0f;
        Vector3 axisZ{ 0.0f, 0.0f, 1.0f };
        float padding2 = 0.0f;

        Vector3 bodyCenter{};
        float padding3 = 0.0f;
        Vector3 linearVelocity{};
        float padding4 = 0.0f;
        Vector3 angularVelocity{};
        float padding5 = 0.0f;
    };
    static_assert(sizeof(GpuProxy) == 144);

    struct GpuReaction
    {
        int32_t impulseX = 0;
        int32_t impulseY = 0;
        int32_t impulseZ = 0;
        int32_t padding0 = 0;
        int32_t torqueX = 0;
        int32_t torqueY = 0;
        int32_t torqueZ = 0;
        int32_t padding1 = 0;
    };
    static_assert(sizeof(GpuReaction) == 32);

    struct InteractionConstants
    {
        uint32_t activeParticleCount = 0;
        uint32_t proxyCount = 0;
        uint32_t bodyCount = 0;
        uint32_t accumulateReaction = 0;

        float particleMass = 0.0f;
        float particleRadius = 0.0f;
        float couplingStrength = 0.0f;
        float collisionRestitution = 0.0f;

        float collisionFriction = 0.0f;
        float deltaTime = 0.0f;
        float impulseScale = 0.0f;
        float padding0 = 0.0f;
    };
    static_assert(sizeof(InteractionConstants) == 48);

    struct FrameSlot
    {
        ComPtr<ID3D12Resource> proxyUploadBuffer{};
        ComPtr<ID3D12Resource> reactionBuffer{};
        ComPtr<ID3D12Resource> readbackBuffer{};
        GpuProxy* mappedProxyData = nullptr;
        std::array<ActorHandle, kMaxDynamicBodies> bodyHandles{};
        D3D12_RESOURCE_STATES reactionState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        uint32_t proxyCount = 0;
        uint32_t bodyCount = 0;
        uint32_t pendingBodyCount = 0;
        bool pendingReadback = false;
    };

    static constexpr float kImpulseFixedPointScale = 1000.0f;

    GpuSphRigidbodyInteraction() = default;
    ~GpuSphRigidbodyInteraction() = default;
    GpuSphRigidbodyInteraction(const GpuSphRigidbodyInteraction&) = delete;
    GpuSphRigidbodyInteraction& operator=(const GpuSphRigidbodyInteraction&) = delete;

    bool EnsureInitialized()
    {
        if (initialized_)
        {
            return true;
        }

        dxCommon_ = DirectXCommon::GetInstance();
        if (!dxCommon_ || !dxCommon_->GetDevice() || !dxCommon_->GetCommandManager())
        {
            return false;
        }

        if (!CreateRootSignature() || !CreatePipelineStates())
        {
            Finalize();
            return false;
        }

        const uint32_t frameCount = (std::max)(1u, dxCommon_->GetCommandManager()->GetFrameResourceCount());
        frameSlots_.resize(frameCount);
        for (uint32_t index = 0; index < frameCount; ++index)
        {
            if (!CreateFrameSlot(frameSlots_[index], index))
            {
                Finalize();
                return false;
            }
        }

        initialized_ = true;
        stats_.initialized = true;
        stats_.frameResourceCount = frameCount;
        return true;
    }

    bool CreateRootSignature()
    {
        D3D12_ROOT_PARAMETER parameters[4]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[0].Descriptor.ShaderRegister = 0;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[1].Descriptor.ShaderRegister = 0;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[2].Descriptor.ShaderRegister = 0;
        parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[3].Descriptor.ShaderRegister = 1;
        parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = _countof(parameters);
        desc.pParameters = parameters;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob)))
        {
            return false;
        }

        if (FAILED(dxCommon_->GetDevice()->CreateRootSignature(
            0,
            signatureBlob->GetBufferPointer(),
            signatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_))))
        {
            return false;
        }
        rootSignature_->SetName(L"GpuSph.W9.RigidbodyInteraction.RootSignature");
        return true;
    }

    bool CreatePipelineStates()
    {
        const ShaderDescriptor clearDesc{
            L"GpuSphRigidbodyInteractionClearCS",
            L"Resources/Shaders/GpuFluid/Sph/GpuSphRigidbodyInteraction.CS.hlsl",
            L"ClearReactions",
            L"cs_6_0",
            ShaderStage::Compute,
            RootSignatureType::Compute };
        const ShaderDescriptor collisionDesc{
            L"GpuSphRigidbodyInteractionResolveCS",
            L"Resources/Shaders/GpuFluid/Sph/GpuSphRigidbodyInteraction.CS.hlsl",
            L"ResolveParticles",
            L"cs_6_0",
            ShaderStage::Compute,
            RootSignatureType::Compute };

        const ComPtr<IDxcBlob> clearShader = ShaderCompiler::CompileShader(clearDesc, dxCommon_->GetDXCCompilerManager());
        const ComPtr<IDxcBlob> collisionShader = ShaderCompiler::CompileShader(collisionDesc, dxCommon_->GetDXCCompilerManager());
        if (!clearShader || !collisionShader)
        {
            return false;
        }

        D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
        pipelineDesc.pRootSignature = rootSignature_.Get();
        pipelineDesc.CS = { clearShader->GetBufferPointer(), clearShader->GetBufferSize() };
        if (FAILED(dxCommon_->GetDevice()->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&clearPipelineState_))))
        {
            return false;
        }

        pipelineDesc.CS = { collisionShader->GetBufferPointer(), collisionShader->GetBufferSize() };
        if (FAILED(dxCommon_->GetDevice()->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&collisionPipelineState_))))
        {
            return false;
        }

        clearPipelineState_->SetName(L"GpuSph.W9.ClearReactionPSO");
        collisionPipelineState_->SetName(L"GpuSph.W9.RigidbodyCollisionPSO");
        return true;
    }

    bool CreateFrameSlot(FrameSlot& slot, uint32_t frameIndex)
    {
        const uint64_t proxyBytes = static_cast<uint64_t>(sizeof(GpuProxy)) * kMaxProxies;
        const uint64_t reactionBytes = static_cast<uint64_t>(sizeof(GpuReaction)) * kMaxDynamicBodies;

        slot.proxyUploadBuffer = ResourceManager::CreateBufferResource(
            dxCommon_->GetDevice(), proxyBytes, D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
        slot.reactionBuffer = ResourceManager::CreateBufferResource(
            dxCommon_->GetDevice(), reactionBytes, D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        slot.readbackBuffer = ResourceManager::CreateBufferResource(
            dxCommon_->GetDevice(), reactionBytes, D3D12_HEAP_TYPE_READBACK,
            D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
        if (!slot.proxyUploadBuffer || !slot.reactionBuffer || !slot.readbackBuffer)
        {
            return false;
        }

        const std::wstring suffix = std::to_wstring(frameIndex);
        slot.proxyUploadBuffer->SetName((L"GpuSph.W9.ProxyUpload." + suffix).c_str());
        slot.reactionBuffer->SetName((L"GpuSph.W9.Reaction." + suffix).c_str());
        slot.readbackBuffer->SetName((L"GpuSph.W9.ReactionReadback." + suffix).c_str());
        slot.reactionState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        if (FAILED(slot.proxyUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&slot.mappedProxyData))))
        {
            return false;
        }
        return slot.mappedProxyData != nullptr;
    }

    void GatherRigidbodyProxies(ActorWorld& actorWorld, FrameSlot& slot)
    {
        slot.proxyCount = 0;
        slot.bodyCount = 0;
        if (!slot.mappedProxyData)
        {
            return;
        }

        for (const std::unique_ptr<Actor>& actorOwner : actorWorld.GetActors())
        {
            Actor* actor = actorOwner.get();
            if (!actor || actor->IsPendingDestroy() || !actor->IsActive())
            {
                continue;
            }

            RigidbodyComponent* rigidbodyComponent = actor->GetComponent<RigidbodyComponent>();
            Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
            if (!rigidbody)
            {
                continue;
            }

            uint32_t bodyIndex = UINT32_MAX;
            if (rigidbody->GetBodyType() == BodyType::Dynamic && slot.bodyCount < kMaxDynamicBodies)
            {
                bodyIndex = slot.bodyCount;
                slot.bodyHandles[slot.bodyCount] = actorWorld.MakeActorHandle(actor);
                ++slot.bodyCount;
            }

            const Vector3 bodyCenter = actor->GetRootComponent()
                ? actor->GetRootComponent()->GetWorldPosition()
                : Vector3{};

            for (ColliderComponent* colliderComponent : actor->GetComponents<ColliderComponent>())
            {
                if (slot.proxyCount >= kMaxProxies)
                {
                    break;
                }
                Collider* collider = colliderComponent ? colliderComponent->GetCollider() : nullptr;
                if (!collider || !collider->IsEnabled() || !collider->IsPhysicsEnabled() || collider->IsTrigger())
                {
                    continue;
                }

                GpuProxy proxy{};
                proxy.bodyIndex = bodyIndex;
                proxy.restitution = rigidbody->GetRestitution();
                proxy.friction = rigidbody->GetDynamicFriction();
                proxy.bodyCenter = actor->GetRootComponent() ? bodyCenter : collider->GetCenterPosition();
                proxy.linearVelocity = rigidbody->GetVelocity();
                proxy.angularVelocity = rigidbody->GetAngularVelocity();

                bool supported = true;
                switch (collider->GetShapeType())
                {
                case ECollisionShapeType::Sphere:
                {
                    const Sphere sphere = collider->GetSphere();
                    proxy.shapeType = static_cast<uint32_t>(ProxyShape::Sphere);
                    proxy.center = sphere.center;
                    proxy.radius = sphere.radius;
                    break;
                }
                case ECollisionShapeType::AABB:
                {
                    const AABB aabb = collider->GetAABB();
                    proxy.shapeType = static_cast<uint32_t>(ProxyShape::Aabb);
                    proxy.center = (aabb.min + aabb.max) * 0.5f;
                    proxy.halfSize = (aabb.max - aabb.min) * 0.5f;
                    break;
                }
                case ECollisionShapeType::OBB:
                {
                    const OBB obb = collider->GetOBB();
                    proxy.shapeType = static_cast<uint32_t>(ProxyShape::Obb);
                    proxy.center = obb.center;
                    proxy.halfSize = obb.size;
                    proxy.axisX = obb.orientations[0];
                    proxy.axisY = obb.orientations[1];
                    proxy.axisZ = obb.orientations[2];
                    break;
                }
                case ECollisionShapeType::Capsule:
                {
                    const Capsule capsule = collider->GetCapsule();
                    proxy.shapeType = static_cast<uint32_t>(ProxyShape::Capsule);
                    proxy.center = capsule.GetCenter();
                    proxy.radius = capsule.radius;
                    proxy.capsuleHalfLength = capsule.GetHeight() * 0.5f;
                    proxy.axisY = capsule.GetAxis();
                    break;
                }
                default:
                    supported = false;
                    break;
                }

                if (!supported)
                {
                    continue;
                }
                slot.mappedProxyData[slot.proxyCount++] = proxy;
            }
        }
    }

    void ConsumeCompletedReadback(ActorWorld& actorWorld, FrameSlot& slot)
    {
        if (!slot.pendingReadback || !slot.readbackBuffer || slot.pendingBodyCount == 0)
        {
            return;
        }

        GpuReaction* mappedReaction = nullptr;
        const SIZE_T readBytes = static_cast<SIZE_T>(slot.pendingBodyCount) * sizeof(GpuReaction);
        D3D12_RANGE readRange{ 0, readBytes };
        if (FAILED(slot.readbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedReaction))) || !mappedReaction)
        {
            slot.pendingReadback = false;
            return;
        }

        stats_.lastLinearImpulse = 0.0f;
        stats_.lastAngularImpulse = 0.0f;
        for (uint32_t bodyIndex = 0; bodyIndex < slot.pendingBodyCount && bodyIndex < kMaxDynamicBodies; ++bodyIndex)
        {
            Actor* actor = actorWorld.ResolveActor(slot.bodyHandles[bodyIndex]);
            RigidbodyComponent* rigidbodyComponent = actor ? actor->GetComponent<RigidbodyComponent>() : nullptr;
            Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
            if (!rigidbody || rigidbody->GetBodyType() != BodyType::Dynamic)
            {
                continue;
            }

            Vector3 linearImpulse{
                static_cast<float>(mappedReaction[bodyIndex].impulseX) / kImpulseFixedPointScale,
                static_cast<float>(mappedReaction[bodyIndex].impulseY) / kImpulseFixedPointScale,
                static_cast<float>(mappedReaction[bodyIndex].impulseZ) / kImpulseFixedPointScale };
            Vector3 angularImpulse{
                static_cast<float>(mappedReaction[bodyIndex].torqueX) / kImpulseFixedPointScale,
                static_cast<float>(mappedReaction[bodyIndex].torqueY) / kImpulseFixedPointScale,
                static_cast<float>(mappedReaction[bodyIndex].torqueZ) / kImpulseFixedPointScale };

            linearImpulse = ClampMagnitude(linearImpulse, settings_.maximumLinearImpulse);
            angularImpulse = ClampMagnitude(angularImpulse, settings_.maximumAngularImpulse);

            if (Vector3::LengthSquared(linearImpulse) > 0.0f)
            {
                rigidbody->SetVelocity(rigidbody->GetVelocity() + linearImpulse * rigidbody->GetInvMass());
                stats_.lastLinearImpulse = Vector3::Length(linearImpulse);
            }
            if (Vector3::LengthSquared(angularImpulse) > 0.0f)
            {
                const Vector3 invInertia = rigidbody->GetInvInertia();
                rigidbody->SetAngularVelocity(rigidbody->GetAngularVelocity() + Vector3{
                    angularImpulse.x * invInertia.x,
                    angularImpulse.y * invInertia.y,
                    angularImpulse.z * invInertia.z });
                stats_.lastAngularImpulse = Vector3::Length(angularImpulse);
            }
            ++stats_.appliedBodyCount;
        }

        D3D12_RANGE writeRange{ 0, 0 };
        slot.readbackBuffer->Unmap(0, &writeRange);
        slot.pendingReadback = false;
        slot.pendingBodyCount = 0;
        ++stats_.readbackCount;
    }

    void BindCommonResources(
        ID3D12GraphicsCommandList* commandList,
        const FrameSlot& slot,
        GpuSphParticleBuffer& particleBuffer,
        D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress,
        ID3D12PipelineState* pipelineState)
    {
        commandList->SetComputeRootSignature(rootSignature_.Get());
        commandList->SetPipelineState(pipelineState);
        commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
        commandList->SetComputeRootUnorderedAccessView(1, particleBuffer.GetResource()->GetGPUVirtualAddress());
        commandList->SetComputeRootShaderResourceView(2, slot.proxyUploadBuffer->GetGPUVirtualAddress());
        commandList->SetComputeRootUnorderedAccessView(3, slot.reactionBuffer->GetGPUVirtualAddress());
    }

    void TransitionReactionBuffer(
        ID3D12GraphicsCommandList* commandList,
        FrameSlot& slot,
        D3D12_RESOURCE_STATES newState)
    {
        if (!commandList || !slot.reactionBuffer || slot.reactionState == newState)
        {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = slot.reactionBuffer.Get();
        barrier.Transition.StateBefore = slot.reactionState;
        barrier.Transition.StateAfter = newState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        slot.reactionState = newState;
    }

    static void InsertUavBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource)
    {
        if (!commandList || !resource)
        {
            return;
        }
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = resource;
        commandList->ResourceBarrier(1, &barrier);
    }

    [[nodiscard]] float CalculateParticleRadius(const GpuSphSimulationSettings& settings) const
    {
        return (std::max)(settings_.minimumParticleRadius,
            (std::max)(settings.spawnSpacing, 0.001f) * (std::max)(settings_.particleRadiusScale, 0.01f));
    }

    static Vector3 ClampMagnitude(const Vector3& value, float maxMagnitude)
    {
        if (maxMagnitude <= 0.0f)
        {
            return {};
        }
        const float lengthSquared = Vector3::LengthSquared(value);
        const float maxSquared = maxMagnitude * maxMagnitude;
        if (lengthSquared <= maxSquared || lengthSquared <= 1.0e-12f)
        {
            return value;
        }
        return value * (maxMagnitude / std::sqrt(lengthSquared));
    }

private:
    DirectXCommon* dxCommon_ = nullptr;
    ComPtr<ID3D12RootSignature> rootSignature_{};
    ComPtr<ID3D12PipelineState> clearPipelineState_{};
    ComPtr<ID3D12PipelineState> collisionPipelineState_{};
    std::vector<FrameSlot> frameSlots_{};
    GpuSphRigidbodyInteractionSettings settings_{};
    GpuSphRigidbodyInteractionStats stats_{};
    bool initialized_ = false;
};

} // namespace Ken4lowEngine

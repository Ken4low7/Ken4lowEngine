#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// CPU側のPass依存関係を宣言・検証・並べ替えする軽量Render Graph。
	/// D3D12 Resource所有は既存Managerへ残し、Phase 6では実行順とResource lifetimeを管理する。
	/// </summary>
	class RenderGraph
	{
	public:
		struct ResourceHandle
		{
			static constexpr uint32_t kInvalid = (std::numeric_limits<uint32_t>::max)();
			uint32_t id = kInvalid;
			[[nodiscard]] bool IsValid() const { return id != kInvalid; }
			friend bool operator==(ResourceHandle left, ResourceHandle right) { return left.id == right.id; }
		};

		struct PassHandle
		{
			static constexpr uint32_t kInvalid = (std::numeric_limits<uint32_t>::max)();
			uint32_t id = kInvalid;
			[[nodiscard]] bool IsValid() const { return id != kInvalid; }
			friend bool operator==(PassHandle left, PassHandle right) { return left.id == right.id; }
		};

		struct ResourceLifetime
		{
			std::size_t firstPass = (std::numeric_limits<std::size_t>::max)();
			std::size_t lastPass = 0;
			bool imported = false;
		};

		struct CompileStats
		{
			std::size_t passCount = 0;
			std::size_t resourceCount = 0;
			std::size_t dependencyCount = 0;
			std::size_t transientResourceCount = 0;
		};

		using ExecuteCallback = std::function<void()>;

		void Reset();
		ResourceHandle CreateResource(std::string name, bool imported = false);
		PassHandle AddPass(
			std::string name,
			std::vector<ResourceHandle> reads,
			std::vector<ResourceHandle> writes,
			ExecuteCallback execute);
		PassHandle AddPass(
			std::string name,
			std::initializer_list<ResourceHandle> reads,
			std::initializer_list<ResourceHandle> writes,
			ExecuteCallback execute);
		bool AddDependency(PassHandle before, PassHandle after);

		bool Compile(std::string* outError = nullptr);
		bool Execute(std::string* outError = nullptr);

		[[nodiscard]] const CompileStats& GetCompileStats() const { return compileStats_; }
		[[nodiscard]] const ResourceLifetime* GetResourceLifetime(ResourceHandle handle) const;
		[[nodiscard]] std::string_view GetPassName(PassHandle handle) const;
		[[nodiscard]] std::size_t GetCompiledPassCount() const { return compiledOrder_.size(); }

	private:
		struct ResourceNode
		{
			std::string name;
			ResourceLifetime lifetime{};
		};

		struct PassNode
		{
			std::string name;
			std::vector<ResourceHandle> reads;
			std::vector<ResourceHandle> writes;
			std::vector<uint32_t> explicitDependencies;
			ExecuteCallback execute;
		};

		bool ValidateResourceHandle(ResourceHandle handle) const;

		std::vector<ResourceNode> resources_;
		std::vector<PassNode> passes_;
		std::vector<uint32_t> compiledOrder_;
		CompileStats compileStats_{};
		bool compiled_ = false;
	};
} // namespace Ken4lowEngine

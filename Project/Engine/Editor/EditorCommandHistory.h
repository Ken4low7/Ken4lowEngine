#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>Undo / Redoへ登録できるEditor操作の共通インターフェースです。</summary>
	class IEditorCommand
	{
	public:
		virtual ~IEditorCommand() = default;
		virtual void Execute() = 0;
		virtual void Undo() = 0;
		virtual const std::string& GetName() const = 0;
	};

	/// <summary>任意の実行・取り消し処理をCommandとして保持します。</summary>
	class EditorLambdaCommand final : public IEditorCommand
	{
	public:
		using Action = std::function<void()>;

		EditorLambdaCommand(std::string name, Action execute, Action undo)
			: name_(std::move(name)), execute_(std::move(execute)), undo_(std::move(undo)) {}

		void Execute() override
		{
			if (execute_) execute_(); // Redo時も初回実行と同じ処理を呼び出す。
		}

		void Undo() override
		{
			if (undo_) undo_();
		}

		const std::string& GetName() const override { return name_; }

	private:
		std::string name_;
		Action execute_;
		Action undo_;
	};

	/// <summary>値の変更前後と書き戻し関数を保持する汎用Commandです。</summary>
	template<class TValue>
	class EditorValueCommand final : public IEditorCommand
	{
	public:
		using Apply = std::function<void(const TValue&)>;

		EditorValueCommand(std::string name, TValue before, TValue after, Apply apply)
			: name_(std::move(name)), before_(std::move(before)), after_(std::move(after)), apply_(std::move(apply)) {}

		void Execute() override
		{
			if (apply_) apply_(after_);
		}

		void Undo() override
		{
			if (apply_) apply_(before_);
		}

		const std::string& GetName() const override { return name_; }

	private:
		std::string name_;
		TValue before_{};
		TValue after_{};
		Apply apply_;
	};

	/// <summary>文字列化したInspector状態を復元するCommandです。</summary>
	class EditorStateCommand final : public IEditorCommand
	{
	public:
		using Apply = std::function<void(std::string_view)>;

		EditorStateCommand(std::string name, std::string before, std::string after, Apply apply)
			: name_(std::move(name)), before_(std::move(before)), after_(std::move(after)), apply_(std::move(apply)) {}

		void Execute() override
		{
			if (apply_) apply_(after_);
		}

		void Undo() override
		{
			if (apply_) apply_(before_);
		}

		const std::string& GetName() const override { return name_; }

	private:
		std::string name_;
		std::string before_;
		std::string after_;
		Apply apply_;
	};

	/// <summary>複数のEditor操作を一つのUndo / Redo単位として扱うCommandです。</summary>
	class EditorCompositeCommand final : public IEditorCommand
	{
	public:
		EditorCompositeCommand(std::string name, std::vector<std::unique_ptr<IEditorCommand>> commands)
			: name_(std::move(name)), commands_(std::move(commands)) {}

		void Execute() override
		{
			for (const std::unique_ptr<IEditorCommand>& command : commands_)
			{
				if (command) command->Execute();
			}
		}

		void Undo() override
		{
			// Transactionは実行順と逆順に戻し、複数Property編集でも一つの原子的な履歴として扱う。
			for (auto it = commands_.rbegin(); it != commands_.rend(); ++it)
			{
				if (*it) (*it)->Undo();
			}
		}

		const std::string& GetName() const override { return name_; }
		std::size_t GetCommandCount() const { return commands_.size(); }

	private:
		std::string name_;
		std::vector<std::unique_ptr<IEditorCommand>> commands_;
	};

	/// <summary>Editor全体のCommand履歴とUndo / Redo位置を管理します。</summary>
	class EditorCommandHistory
	{
	public:
		static EditorCommandHistory* GetInstance()
		{
			static EditorCommandHistory instance;
			return &instance;
		}

		void Execute(std::unique_ptr<IEditorCommand> command)
		{
			if (!command || isReplaying_) return;
			command->Execute();
			AppendExecutedCommand(std::move(command));
		}

		void PushExecuted(std::unique_ptr<IEditorCommand> command)
		{
			if (!command || isReplaying_) return;
			AppendExecutedCommand(std::move(command)); // 既に画面へ反映済みのドラッグ操作は再実行せず履歴だけ追加する。
		}

		bool BeginTransaction(std::string name)
		{
			if (isReplaying_ || transactionActive_) return false;
			transactionActive_ = true;
			transactionName_ = name.empty() ? "Editor Transaction" : std::move(name);
			transactionCommands_.clear();
			return true;
		}

		bool CommitTransaction()
		{
			if (!transactionActive_ || isReplaying_) return false;

			std::string name = std::move(transactionName_);
			std::vector<std::unique_ptr<IEditorCommand>> commands = std::move(transactionCommands_);
			ResetTransaction();
			if (commands.empty()) return false;

			PushInternal(std::make_unique<EditorCompositeCommand>(std::move(name), std::move(commands)));
			return true;
		}

		bool CancelTransaction()
		{
			if (!transactionActive_ || isReplaying_) return false;

			std::vector<std::unique_ptr<IEditorCommand>> commands = std::move(transactionCommands_);
			ResetTransaction();
			ReplayScope replayScope{ *this };
			for (auto it = commands.rbegin(); it != commands.rend(); ++it)
			{
				if (*it) (*it)->Undo();
			}
			return true;
		}

		bool Undo()
		{
			if (!CanUndo()) return false;

			const std::size_t previousCursor = cursor_;
			ReplayScope replayScope{ *this };
			--cursor_;
			try
			{
				commands_[cursor_]->Undo();
			}
			catch (...)
			{
				cursor_ = previousCursor; // 失敗したUndoを履歴上だけ成功扱いにしない。
				throw;
			}
			return true;
		}

		bool Redo()
		{
			if (!CanRedo()) return false;

			ReplayScope replayScope{ *this };
			commands_[cursor_]->Execute();
			++cursor_;
			return true;
		}

		void Clear()
		{
			commands_.clear();
			cursor_ = 0;
			ResetTransaction();
		}

		void DiscardDependentRedoCommands()
		{
			if (!isReplaying_ || cursor_ >= commands_.size()) return;
			const std::size_t keepCount = cursor_ + 1;
			commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(keepCount), commands_.end());
			// 構造変更前のComponentを参照する後続Redoだけを捨て、構造Command自身は再実行できるように残す。
		}

		bool CanUndo() const { return !transactionActive_ && cursor_ > 0 && cursor_ <= commands_.size(); }
		bool CanRedo() const { return !transactionActive_ && cursor_ < commands_.size(); }
		bool IsReplaying() const { return isReplaying_; }
		bool IsTransactionActive() const { return transactionActive_; }
		std::size_t GetUndoCount() const { return cursor_; }
		std::size_t GetRedoCount() const { return commands_.size() - cursor_; }
		std::size_t GetHistorySize() const { return commands_.size(); }
		std::size_t GetTransactionCommandCount() const { return transactionCommands_.size(); }
		std::size_t GetCapacity() const { return capacity_; }
		const std::string& GetTransactionName() const { return transactionName_; }

		const char* GetUndoName() const
		{
			return CanUndo() ? commands_[cursor_ - 1]->GetName().c_str() : nullptr;
		}

		const char* GetRedoName() const
		{
			return CanRedo() ? commands_[cursor_]->GetName().c_str() : nullptr;
		}

		void SetCapacity(std::size_t capacity)
		{
			capacity_ = std::max<std::size_t>(1, capacity);
			TrimToCapacity();
		}

	private:
		class ReplayScope final
		{
		public:
			explicit ReplayScope(EditorCommandHistory& history) : history_(history)
			{
				history_.isReplaying_ = true;
			}

			~ReplayScope()
			{
				history_.isReplaying_ = false;
			}

			ReplayScope(const ReplayScope&) = delete;
			ReplayScope& operator=(const ReplayScope&) = delete;

		private:
			EditorCommandHistory& history_;
		};

		EditorCommandHistory() = default;

		void AppendExecutedCommand(std::unique_ptr<IEditorCommand> command)
		{
			if (transactionActive_)
			{
				transactionCommands_.push_back(std::move(command));
				return;
			}
			PushInternal(std::move(command));
		}

		void PushInternal(std::unique_ptr<IEditorCommand> command)
		{
			if (cursor_ < commands_.size())
			{
				commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());
			}
			commands_.push_back(std::move(command));
			cursor_ = commands_.size();
			TrimToCapacity();
		}

		void TrimToCapacity()
		{
			if (commands_.size() <= capacity_) return;
			const std::size_t removeCount = commands_.size() - capacity_;
			commands_.erase(commands_.begin(), commands_.begin() + static_cast<std::ptrdiff_t>(removeCount));
			cursor_ = cursor_ > removeCount ? cursor_ - removeCount : 0;
		}

		void ResetTransaction()
		{
			transactionCommands_.clear();
			transactionName_.clear();
			transactionActive_ = false;
		}

		std::vector<std::unique_ptr<IEditorCommand>> commands_;
		std::vector<std::unique_ptr<IEditorCommand>> transactionCommands_;
		std::string transactionName_;
		std::size_t cursor_ = 0;
		std::size_t capacity_ = 256;
		bool isReplaying_ = false;
		bool transactionActive_ = false;
	};
} // namespace Ken4lowEngine

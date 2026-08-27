#include "EditorCommandHistory.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using Ken4lowEngine::EditorCommandHistory;
using Ken4lowEngine::EditorLambdaCommand;
using Ken4lowEngine::IEditorCommand;

namespace
{
	class ThrowOnceUndoCommand final : public IEditorCommand
	{
	public:
		explicit ThrowOnceUndoCommand(int& value) : value_(value) {}

		void Execute() override { ++value_; }
		void Undo() override
		{
			if (throwOnUndo_)
			{
				throwOnUndo_ = false;
				throw std::runtime_error("undo failed");
			}
			--value_;
		}
		const std::string& GetName() const override { return name_; }

	private:
		int& value_;
		bool throwOnUndo_ = true;
		std::string name_ = "ThrowOnceUndo";
	};

	class ThrowOnceRedoCommand final : public IEditorCommand
	{
	public:
		explicit ThrowOnceRedoCommand(int& value) : value_(value) {}

		void Execute() override
		{
			if (hasExecutedOnce_ && throwOnRedo_)
			{
				throwOnRedo_ = false;
				throw std::runtime_error("redo failed");
			}
			hasExecutedOnce_ = true;
			++value_;
		}
		void Undo() override { --value_; }
		const std::string& GetName() const override { return name_; }

	private:
		int& value_;
		bool hasExecutedOnce_ = false;
		bool throwOnRedo_ = true;
		std::string name_ = "ThrowOnceRedo";
	};

	std::unique_ptr<IEditorCommand> MakeDeltaCommand(int& value, int delta, const char* name)
	{
		return std::make_unique<EditorLambdaCommand>(
			name,
			[&value, delta]() { value += delta; },
			[&value, delta]() { value -= delta; });
	}

	void ResetHistory()
	{
		EditorCommandHistory* history = EditorCommandHistory::GetInstance();
		history->Clear();
		history->SetCapacity(256);
	}

	void TestBasicUndoRedoAndRedoTruncation()
	{
		ResetHistory();
		EditorCommandHistory* history = EditorCommandHistory::GetInstance();
		int value = 0;

		history->Execute(MakeDeltaCommand(value, 3, "Add3"));
		assert(value == 3);
		assert(history->GetUndoCount() == 1);
		assert(history->Undo());
		assert(value == 0);
		assert(history->CanRedo());

		history->Execute(MakeDeltaCommand(value, 7, "Add7"));
		assert(value == 7);
		assert(!history->CanRedo()); // Undo後の新規編集は分岐したRedo履歴を捨てる。
		assert(history->GetHistorySize() == 1);
	}

	void TestTransactionCommitAndReverseUndo()
	{
		ResetHistory();
		EditorCommandHistory* history = EditorCommandHistory::GetInstance();
		int value = 0;

		assert(history->BeginTransaction("Batch Edit"));
		history->Execute(MakeDeltaCommand(value, 1, "Add1"));
		history->Execute(MakeDeltaCommand(value, 10, "Add10"));
		assert(value == 11);
		assert(history->IsTransactionActive());
		assert(history->GetTransactionCommandCount() == 2);
		assert(!history->CanUndo());
		assert(history->CommitTransaction());
		assert(history->GetHistorySize() == 1);
		assert(std::string(history->GetUndoName()) == "Batch Edit");

		assert(history->Undo());
		assert(value == 0);
		assert(history->Redo());
		assert(value == 11);
	}

	void TestTransactionCancelRestoresStateWithoutHistory()
	{
		ResetHistory();
		EditorCommandHistory* history = EditorCommandHistory::GetInstance();
		int value = 5;

		assert(history->BeginTransaction("Cancelled Edit"));
		history->Execute(MakeDeltaCommand(value, 2, "Add2"));
		history->Execute(MakeDeltaCommand(value, 4, "Add4"));
		assert(value == 11);
		assert(history->CancelTransaction());
		assert(value == 5);
		assert(!history->IsTransactionActive());
		assert(history->GetHistorySize() == 0);
	}

	void TestCapacityTrimsOldestCommands()
	{
		ResetHistory();
		EditorCommandHistory* history = EditorCommandHistory::GetInstance();
		int value = 0;
		history->SetCapacity(2);

		history->Execute(MakeDeltaCommand(value, 1, "One"));
		history->Execute(MakeDeltaCommand(value, 2, "Two"));
		history->Execute(MakeDeltaCommand(value, 4, "Four"));
		assert(history->GetHistorySize() == 2);
		assert(history->GetUndoCount() == 2);
		assert(history->Undo());
		assert(history->Undo());
		assert(value == 1); // Capacity外へ落ちた最古Commandの結果だけは現在World状態として残る。
	}

	void TestUndoFailureRestoresCursorAndReplayFlag()
	{
		ResetHistory();
		EditorCommandHistory* history = EditorCommandHistory::GetInstance();
		int value = 0;
		history->Execute(std::make_unique<ThrowOnceUndoCommand>(value));
		assert(value == 1);

		bool caught = false;
		try
		{
			history->Undo();
		}
		catch (const std::runtime_error&)
		{
			caught = true;
		}
		assert(caught);
		assert(!history->IsReplaying());
		assert(history->GetUndoCount() == 1);
		assert(history->CanUndo());
		assert(history->Undo());
		assert(value == 0);
	}

	void TestRedoFailureKeepsRedoAvailable()
	{
		ResetHistory();
		EditorCommandHistory* history = EditorCommandHistory::GetInstance();
		int value = 0;
		history->Execute(std::make_unique<ThrowOnceRedoCommand>(value));
		assert(history->Undo());
		assert(value == 0);

		bool caught = false;
		try
		{
			history->Redo();
		}
		catch (const std::runtime_error&)
		{
			caught = true;
		}
		assert(caught);
		assert(!history->IsReplaying());
		assert(history->GetRedoCount() == 1);
		assert(history->CanRedo());
		assert(history->Redo());
		assert(value == 1);
	}
}

int main()
{
	TestBasicUndoRedoAndRedoTruncation();
	TestTransactionCommitAndReverseUndo();
	TestTransactionCancelRestoresStateWithoutHistory();
	TestCapacityTrimsOldestCommands();
	TestUndoFailureRestoresCursorAndReplayFlag();
	TestRedoFailureKeepsRedoAvailable();
	std::cout << "Editor Command History runtime tests passed\n";
	return 0;
}

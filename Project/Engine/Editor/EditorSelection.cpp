#include "EditorSelection.h"

namespace Ken4lowEngine
{

	void EditorSelection::Select(const EditorObjectInfo& objectInfo)
	{
		// 選択はコピーした軽量情報だけを保持し、Scene側オブジェクト寿命に依存しないようにする。
		selected_ = objectInfo;
		hasSelection_ = true;
	}

	void EditorSelection::RefreshSelected(const EditorObjectInfo& objectInfo)
	{
		// 選択IDを維持したまま、毎フレーム再収集された編集入口へ差し替える。
		selected_ = objectInfo;
		hasSelection_ = true;
	}

	void EditorSelection::Clear()
	{
		// Scene切り替え時の古い選択を安全に破棄できる入口にする。
		selected_ = {};
		hasSelection_ = false;
	}

	bool EditorSelection::HasSelection() const
	{
		return hasSelection_;
	}

	const EditorObjectInfo& EditorSelection::GetSelected() const
	{
		return selected_;
	}

} // namespace Ken4lowEngine

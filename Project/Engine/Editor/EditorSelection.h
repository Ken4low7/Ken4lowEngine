#pragma once

#include "EditorObjectInfo.h"

namespace Ken4lowEngine
{

	/// <summary>
	/// World Outlinerで選択したオブジェクト情報をDetailsへ橋渡しします。
	/// </summary>
	class EditorSelection
	{
	public:
		void Select(const EditorObjectInfo& objectInfo);
		void Clear();
		void RefreshSelected(const EditorObjectInfo& objectInfo);
		bool HasSelection() const;
		const EditorObjectInfo& GetSelected() const;

	private:
		bool hasSelection_ = false;
		EditorObjectInfo selected_{};
	};

} // namespace Ken4lowEngine

from pathlib import Path

path = Path('Project/Engine/Core/Application/GameApplication.cpp')
text = path.read_text(encoding='utf-8-sig')

replacements = [
    (
        '#include "Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h"\n',
        '#include "Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h"\n#include "Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h"\n'
    ),
    (
        '#include "Engine/Vfx/Graph/Editor/VfxGraphEditor.h"\n',
        '#include "Engine/Vfx/Graph/Editor/VfxGraphEditor.h"\n#include "Engine/Vfx/Graph/Editor/VfxDiagnosticsWindow.h"\n'
    ),
    (
        '\t\tVfxCueRuntime::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime(), actorWorld);\n',
        '\t\tVfxCueRuntime::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime(), actorWorld);\n\t\tVfxGraphDiagnostics::GetInstance()->CaptureFrame(); // Phase28 samples existing runtime counters without a GPU fence wait.\n'
    ),
    (
        '\t\t\t\t\tVfxGraphEditor::GetInstance()->Draw(&editorWindowState.showVfxGraphEditor);\n',
        '\t\t\t\t\tVfxGraphEditor::GetInstance()->Draw(&editorWindowState.showVfxGraphEditor);\n\t\t\t\t\tVfxDiagnosticsWindow::GetInstance()->Draw(editorWindowState.showVfxGraphEditor);\n'
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'Expected exactly one integration target, got {count}: {old!r}')
    text = text.replace(old, new, 1)

path.write_text(text, encoding='utf-8')
print('Phase28 diagnostics integrated into GameApplication.')

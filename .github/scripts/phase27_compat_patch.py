from pathlib import Path

path = Path('Project/Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h')
text = path.read_text(encoding='utf-8-sig')

old_play = '''\tbool Play(const std::string& effectName, const Vector3& worldPosition, float runtimeScale = 1.0f)\n\t{\n\t\truntimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);\n'''
new_play = '''\tbool Play(const std::string& effectName, const Vector3& worldPosition)\n\t{\n\t\treturn Play(effectName, worldPosition, 1.0f);\n\t}\n\n\tbool Play(const std::string& effectName, const Vector3& worldPosition, float runtimeScale)\n\t{\n\t\truntimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);\n'''

old_loop = '''\tPlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition, float runtimeScale = 1.0f)\n\t{\n\t\truntimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);\n'''
new_loop = '''\tPlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition)\n\t{\n\t\treturn PlayLoop(effectName, worldPosition, 1.0f);\n\t}\n\n\tPlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition, float runtimeScale)\n\t{\n\t\truntimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);\n'''

for old, new in ((old_play, new_play), (old_loop, new_loop)):
    if text.count(old) != 1:
        raise RuntimeError(f'Expected exactly one compatibility target, got {text.count(old)}')
    text = text.replace(old, new, 1)

path.write_text(text, encoding='utf-8')
print('Phase13 API compatibility overloads restored.')

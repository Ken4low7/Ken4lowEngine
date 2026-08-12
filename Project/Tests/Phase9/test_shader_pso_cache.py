from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DXC_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Shader" / "Compiler" / "DXCCompilerManager.h"
DXC_SOURCE = PROJECT_ROOT / "Engine" / "Graphics" / "Shader" / "Compiler" / "DXCCompilerManager.cpp"
SHADER_COMPILER = PROJECT_ROOT / "Engine" / "Graphics" / "Shader" / "Compiler" / "ShaderCompiler.cpp"
PIPELINE_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Pipeline" / "PipelineFactory.h"
PIPELINE_SOURCE = PROJECT_ROOT / "Engine" / "Graphics" / "Pipeline" / "PipelineFactory.cpp"


class ShaderAndPsoCacheContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dxc_header = DXC_HEADER.read_text(encoding="utf-8")
        cls.dxc_source = DXC_SOURCE.read_text(encoding="utf-8")
        cls.shader_compiler = SHADER_COMPILER.read_text(encoding="utf-8")
        cls.pipeline_header = PIPELINE_HEADER.read_text(encoding="utf-8")
        cls.pipeline_source = PIPELINE_SOURCE.read_text(encoding="utf-8")

    def test_shader_key_uses_source_entry_profile_and_compile_options(self) -> None:
        self.assertIn("BuildShaderCacheKey", self.dxc_header)
        self.assertIn("AppendShaderSourceTree", self.dxc_source)
        self.assertIn("entryPoint", self.dxc_source)
        self.assertIn("profile", self.dxc_source)
        self.assertIn("-Zi|-Qembed_debug|-Od|-Zpr", self.dxc_source)

    def test_local_include_contents_participate_in_shader_key(self) -> None:
        # Recursive local include expansion makes a changed .hlsli produce a different cache key.
        self.assertIn("ExtractIncludeTokens", self.dxc_source)
        self.assertIn("physicalPath.parent_path() / includeRelative", self.dxc_source)
        self.assertIn("AppendShaderSourceTree(includePhysical", self.dxc_source)
        self.assertIn("<include-cycle-or-repeat>", self.dxc_source)

    def test_shader_compiler_checks_cache_before_dxc_compile(self) -> None:
        cache_lookup = self.shader_compiler.index("FindCachedShader")
        compile_call = self.shader_compiler.index("dxcCompiler->Compile")
        self.assertLess(cache_lookup, compile_call)
        self.assertIn("StoreCachedShader", self.shader_compiler)
        self.assertIn("Shader Cache Hit", self.shader_compiler)

    def test_shader_cache_supports_explicit_invalidation_and_diagnostics(self) -> None:
        self.assertIn("InvalidateShader", self.dxc_header)
        self.assertIn("ClearShaderCache", self.dxc_header)
        for field in ("requestCount", "hitCount", "missCount", "compileCount", "invalidationCount", "entryCount"):
            self.assertIn(field, self.dxc_header)
        self.assertIn("GetShaderCacheStats", self.dxc_header)

    def test_pso_key_is_structural_and_not_root_signature_pointer_identity(self) -> None:
        self.assertIn("BuildGraphicsPipelineCacheKey", self.pipeline_header)
        self.assertIn("serializedRootSignature->GetBufferPointer()", self.pipeline_source)
        self.assertIn("AppendShaderBlob", self.pipeline_source)
        self.assertIn("element.SemanticName", self.pipeline_source)
        self.assertIn("desc.blendState", self.pipeline_source)
        self.assertIn("desc.rasterizerState", self.pipeline_source)
        self.assertIn("desc.depthStencilState", self.pipeline_source)
        self.assertNotIn("reinterpret_cast<uintptr_t>(out.rootSignature", self.pipeline_source)

    def test_pso_cache_lookup_happens_before_d3d12_object_creation(self) -> None:
        lookup = self.pipeline_source.index("graphicsPipelineCache_.find")
        root_create = self.pipeline_source.index("CreateRootSignature", lookup)
        pso_create = self.pipeline_source.index("CreateGraphicsPipelineState", lookup)
        self.assertLess(lookup, root_create)
        self.assertLess(lookup, pso_create)
        self.assertIn("ClearCache", self.pipeline_header)
        self.assertIn("GetCacheStats", self.pipeline_header)


if __name__ == "__main__":
    unittest.main()

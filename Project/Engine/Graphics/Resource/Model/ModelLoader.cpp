#include "ModelLoader.h"
#include "ModelPathResolver.h"
#include "KMeshLoader.h"
#include "AssimpLoader.h"

namespace Ken4lowEngine
{
	ModelData ModelLoader::LoadModel(const std::string& logicalPath)
	{
		// まずは Compiled 側の .kmesh を優先
		if (ModelPathResolver::ExistsCompiled(logicalPath))
		{
			return KMeshLoader::LoadModel(logicalPath);
		}

		// まだ .kmesh 化していないモデルは Sources 側から読む
		return AssimpLoader::LoadModel(logicalPath);
	}
}
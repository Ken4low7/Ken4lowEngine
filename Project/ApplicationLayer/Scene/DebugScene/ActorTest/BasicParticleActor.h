#pragma once
#include "Actor.h"

class BasicParticleActor : public Ken4lowEngine::Actor
{
public:

	void Initialize() override;

	std::string GetClassTypeName() const override
	{
		// JSONやEditor上で使用するActor名を返す
		return "BasicParticle";
	}

	void DrawImGui() override;

private:

	void PlayVFX();
};


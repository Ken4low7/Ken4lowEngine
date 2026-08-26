from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[2]
REGISTRATION = ROOT / "ApplicationLayer/Scene/DebugScene/DebugActorRegistration.cpp"
BASIC_PARTICLE_ACTOR = ROOT / "ApplicationLayer/Scene/DebugScene/ActorTest/BasicParticleActor.h"
BASIC_PARTICLE_PREFAB = ROOT / "Resources/ActorPrefabs/BasicParticleTest.json"


class ApplicationActorRegistrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.registration = REGISTRATION.read_text(encoding="utf-8")
        cls.basic_particle_actor = BASIC_PARTICLE_ACTOR.read_text(encoding="utf-8")
        cls.basic_particle_prefab = json.loads(BASIC_PARTICLE_PREFAB.read_text(encoding="utf-8"))

    def test_basic_particle_class_name_is_registered_for_pie_restore(self) -> None:
        self.assertIn('return "BasicParticle";', self.basic_particle_actor)
        self.assertEqual(self.basic_particle_prefab.get("Class"), "BasicParticle")
        self.assertIn('#include "BasicParticleActor.h"', self.registration)
        self.assertIn('RegisterActorClass<BasicParticleActor>("BasicParticle")', self.registration)


if __name__ == "__main__":
    unittest.main()

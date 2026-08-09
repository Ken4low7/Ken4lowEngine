#define NOMINMAX
#include "AnimationModelDebugView.h"
#include "AnimationModel.h"

#include <Wireframe.h>
#include <unordered_map>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI
#include "Matrix4x4.h"
#include "Vector3.h"

void Ken4lowEngine::AnimationModelDebugView::DrawImGui(AnimationModel& model)
{
#ifdef USE_IMGUI
	// モデルごとにUI状態を保持
	struct UiState
	{
		bool showSkeleton = false;
		bool showColliders = false;
		bool showColliderEditor = false;

		bool thresholdsInitialized = false;
		std::vector<float> thresholds;

		bool lodUpdateEveryInitialized = false;
		std::vector<uint32_t> lodUpdateEvery;
	};

	static std::unordered_map<const AnimationModel*, UiState> s_stateMap;
	UiState& st = s_stateMap[&model];

	ImGui::PushID(&model);

	if (ImGui::Begin("AnimationModel Debug"))
	{
		// ---- Summary ----
		const int lodCount = (int)model.GetLODs().size();
		const int maxLodIndex = (lodCount > 0) ? (lodCount - 1) : 0;

		ImGui::Text("Visible: %s", model.IsVisible() ? "Yes" : "No");
		ImGui::Text("LOD: %d / %d", model.GetLOD(), maxLodIndex);

		// ---- Animation ----
		if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const auto& clips = model.GetAnimationClips();
			int currentIndex = model.GetCurrentAnimationIndex();
			if (!clips.empty())
			{
				const char* previewName = (0 <= currentIndex && currentIndex < (int)clips.size())
					? clips[currentIndex].name.c_str()
					: "(none)";
				if (ImGui::BeginCombo("Clip", previewName))
				{
					for (int i = 0; i < (int)clips.size(); ++i)
					{
						const bool selected = (i == currentIndex);
						if (ImGui::Selectable(clips[i].name.c_str(), selected))
						{
							model.PlayAnimationByIndex((uint32_t)i, true);
							currentIndex = i;
						}
						if (selected) { ImGui::SetItemDefaultFocus(); }
					}
					ImGui::EndCombo();
				}
			}
			else
			{
				ImGui::TextUnformatted("Clip: (none)");
			}

			bool playing = model.IsAnimationPlaying();
			if (ImGui::Checkbox("Playing", &playing))
			{
				model.SetAnimationPlaying(playing);
			}
			ImGui::Text("Time: %.3f / %.3f", model.GetAnimationTime(), model.GetAnimationDurationForDebugBatchTest());
			ImGui::Text("Clip Count: %d", (int)clips.size());
		}

		// ---- Debug Draw ----
		if (ImGui::CollapsingHeader("Debug Draw", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Skeleton wireframe", &st.showSkeleton);
			ImGui::Checkbox("Body-part colliders", &st.showColliders);
			ImGui::Checkbox("Collider editor", &st.showColliderEditor);
		}

		// ---- Transform ----
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (auto* wt = model.GetWorldTransformPtr())
			{
				ImGui::DragFloat3("Translate", reinterpret_cast<float*>(&wt->translate_), 0.01f);
				ImGui::DragFloat3("Rotate", reinterpret_cast<float*>(&wt->rotate_), 0.01f);
				ImGui::DragFloat3("Scale", reinterpret_cast<float*>(&wt->scale_), 0.01f);
			}
		}

		// ---- Rendering / Skinning ----
		if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool hideHead = model.IsHideHead();
			if (ImGui::Checkbox("Hide head", &hideHead))
			{
				model.SetHideHead(hideHead);
			}

			bool computeSkinning = model.IsComputeSkinningEnabled();
			if (ImGui::Checkbox("Compute skinning (CS)", &computeSkinning))
			{
				model.SetComputeSkinningEnabled(computeSkinning);
			}

			float sf = model.GetScaleFactor();
			if (ImGui::DragFloat("ScaleFactor", &sf, 0.01f, 0.01f, 100.0f, "%.2f"))
			{
				model.SetScaleFactor(sf);
			}
		}

		// ---- LOD / Culling ----
		if (ImGui::CollapsingHeader("LOD / Culling", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Force LOD
			bool force = model.IsForceLOD();
			int forcedIndex = model.GetForcedLODIndex();

			if (ImGui::Checkbox("Force LOD", &force))
			{
				model.SetForceLOD(force, forcedIndex);
			}

			if (lodCount > 0)
			{
				int tmp = forcedIndex;
				if (ImGui::SliderInt("Forced Index", &tmp, 0, maxLodIndex))
				{
					forcedIndex = tmp;
					model.SetForceLOD(force, forcedIndex);
				}
			}

			// Cull settings
			float cull = model.GetCullDistance();
			if (ImGui::DragFloat("Cull Distance", &cull, 1.0f, 0.0f, 20000.0f, "%.1f"))
			{
				model.SetCullDistance(cull);
			}

			float extra = model.GetFarCullExtra();
			if (ImGui::DragFloat("Far Cull Extra", &extra, 1.0f, 0.0f, 20000.0f, "%.1f"))
			{
				model.SetFarCullExtra(extra);
			}

			int switchEvery = model.GetLodSwitchUpdateEvery();
			if (ImGui::SliderInt("LOD Switch Update Every (frames)", &switchEvery, 1, 60))
			{
				model.SetLodSwitchUpdateEvery(switchEvery);
			}

			float gap = model.GetLodHysteresisGap();
			if (ImGui::DragFloat("Hysteresis Gap", &gap, 0.1f, 0.0f, 100.0f, "%.2f"))
			{
				model.SetLodHysteresisGap(gap);
			}

			// Threshold editor
			if (!st.thresholdsInitialized)
			{
				st.thresholds = model.GetLodThresholds();
				st.thresholdsInitialized = true;
			}

			ImGui::Separator();
			ImGui::TextUnformatted("Thresholds (distance)");
			ImGui::Text("count: %d (recommended: LODcount-1 = %d)", (int)st.thresholds.size(), (lodCount > 0) ? (lodCount - 1) : 0);

			if (ImGui::Button("Reload##th"))
			{
				st.thresholds = model.GetLodThresholds();
			}
			ImGui::SameLine();
			if (ImGui::Button("Apply##th"))
			{
				model.SetLodThresholds(st.thresholds);
			}
			ImGui::SameLine();
			if (ImGui::Button("+##th"))
			{
				st.thresholds.push_back(10.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("-##th"))
			{
				if (!st.thresholds.empty()) st.thresholds.pop_back();
			}

			for (int i = 0; i < (int)st.thresholds.size(); ++i)
			{
				ImGui::PushID(i);
				ImGui::SetNextItemWidth(160.0f);
				ImGui::InputFloat("##t", &st.thresholds[i], 0.0f, 0.0f, "%.2f");
				ImGui::SameLine();
				ImGui::Text("LOD%d -> LOD%d", i, i + 1);
				ImGui::PopID();
			}

			// LOD update every
			if (!st.lodUpdateEveryInitialized)
			{
				st.lodUpdateEvery = model.GetLodUpdateEvery();
				st.lodUpdateEveryInitialized = true;
			}

			ImGui::Separator();
			ImGui::TextUnformatted("LOD Heavy Update Every (frames)");
			ImGui::Text("count: %d (recommended: LODcount = %d)", (int)st.lodUpdateEvery.size(), lodCount);

			if (ImGui::Button("Reload##ue"))
			{
				st.lodUpdateEvery = model.GetLodUpdateEvery();
			}
			ImGui::SameLine();
			if (ImGui::Button("Apply##ue"))
			{
				model.SetLodUpdateEvery(st.lodUpdateEvery);
			}
			ImGui::SameLine();
			if (ImGui::Button("+##ue"))
			{
				st.lodUpdateEvery.push_back(1);
			}
			ImGui::SameLine();
			if (ImGui::Button("-##ue"))
			{
				if (!st.lodUpdateEvery.empty()) st.lodUpdateEvery.pop_back();
			}

			for (int i = 0; i < (int)st.lodUpdateEvery.size(); ++i)
			{
				ImGui::PushID(1000 + i);
				int v = (int)st.lodUpdateEvery[i];
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::InputInt("##u", &v))
				{
					if (v < 1) v = 1;
					st.lodUpdateEvery[i] = (uint32_t)v;
				}
				ImGui::SameLine();
				ImGui::Text("LOD%d", i);
				ImGui::PopID();
			}
		}

		// ---- Collider editor ----
		if (st.showColliderEditor && ImGui::CollapsingHeader("Body-part Colliders", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& parts = model.GetBodyPartCollidersRef();

			if (ImGui::BeginTable("colliderTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
			{
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Start");
				ImGui::TableSetupColumn("End");
				ImGui::TableSetupColumn("Radius");
				ImGui::TableSetupColumn("Offset");
				ImGui::TableSetupColumn("Height");
				ImGui::TableHeadersRow();

				for (int i = 0; i < (int)parts.size(); ++i)
				{
					auto& p = parts[i];

					ImGui::PushID(i);
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(p.name.c_str());

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("##start", &p.startJointIndex);

					ImGui::TableSetColumnIndex(2);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("##end", &p.endJointIndex);

					ImGui::TableSetColumnIndex(3);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::DragFloat("##r", &p.radius, 0.005f, 0.0f, 10.0f, "%.3f");

					ImGui::TableSetColumnIndex(4);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::DragFloat3("##off", reinterpret_cast<float*>(&p.offset), 0.01f);

					ImGui::TableSetColumnIndex(5);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::DragFloat("##h", &p.height, 0.01f, 0.0f, 1000.0f, "%.2f");

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
		}
	}
	ImGui::End();

	// ウィンドウが閉じられても描画は継続できるよう、Begin/Endの外で実行
#ifdef _DEBUG
	if (st.showSkeleton) { DrawSkeletonWireframe(model); }
	if (st.showColliders) { DrawBodyPartColliders(model); }
#endif

	ImGui::PopID();
	ImGui::End();
#else
	(void)model;
#endif // USE_IMGUI
}

void Ken4lowEngine::AnimationModelDebugView::DrawSkeletonWireframe(AnimationModel& model)
{
#ifdef _DEBUG
	if (!model.GetSkeleton()) { return; }

	const auto& joints = model.GetSkeleton()->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
		model.GetWorldTransformPtr()->scale_,
		model.GetWorldTransformPtr()->rotate_,
		model.GetWorldTransformPtr()->translate_);

	for (const auto& joint : joints)
	{
		if (!joint.parent.has_value()) { continue; }

		const auto& parentJoint = joints[*joint.parent];

		Vector3 parentLocal = parentJoint.skeletonSpaceMatrix.GetTranslation();
		Vector3 jointLocal = joint.skeletonSpaceMatrix.GetTranslation();

		Vector3 parentPos = Vector3::Transform(parentLocal, worldMatrix);
		Vector3 jointPos = Vector3::Transform(jointLocal, worldMatrix);

		Wireframe::GetInstance()->DrawLine(parentPos, jointPos, { 1.0f, 0.0f, 0.0f, 1.0f });
	}
#else
	(void)model;
#endif // _DEBUG
}

void Ken4lowEngine::AnimationModelDebugView::DrawBodyPartColliders(AnimationModel& model)
{
	if (!model.GetSkeleton()) { return; }

	const auto& joints = model.GetSkeleton()->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
		model.GetWorldTransformPtr()->scale_,
		model.GetWorldTransformPtr()->rotate_,
		model.GetWorldTransformPtr()->translate_);

	for (const auto& part : model.GetBodyPartCollidersRef())
	{
		if (part.startJointIndex < 0 || part.startJointIndex >= (int)joints.size()) { continue; }

		if (part.endJointIndex < 0)
		{
			const auto& joint = joints[part.startJointIndex];
			Vector3 localPos = joint.skeletonSpaceMatrix.GetTranslation() + part.offset;
			Vector3 worldPos = Vector3::Transform(localPos, worldMatrix);

			Wireframe::GetInstance()->DrawSphere(worldPos, part.radius, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
		else
		{
			if (part.endJointIndex >= (int)joints.size()) { continue; }

			const auto& jointA = joints[part.startJointIndex];
			const auto& jointB = joints[part.endJointIndex];

			Vector3 a = Vector3::Transform(jointA.skeletonSpaceMatrix.GetTranslation(), worldMatrix);
			Vector3 b = Vector3::Transform(jointB.skeletonSpaceMatrix.GetTranslation(), worldMatrix);

			Vector3 center = (a + b) * 0.5f;
			Vector3 axis = Vector3::Normalize(b - a);
			float height = Vector3::Length(b - a);

			Wireframe::GetInstance()->DrawCapsule(center, part.radius, height, axis, 8, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
	}
}

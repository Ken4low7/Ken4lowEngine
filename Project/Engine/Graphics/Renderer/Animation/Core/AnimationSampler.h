#pragma once
#include <vector>
#include <cassert>
#include <type_traits>

#include "ModelData.h"
#include "Vector3.h"
#include "Quaternion.h"
#include "LinearInterpolation.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				   アニメーションサンプラークラス
	/// -------------------------------------------------------------
	class AnimationSampler
	{
	private: /// ---------- テンプレート定義 ---------- ///

		// コンパイル時に常に false となる型依存定数式
		template<class>
		static constexpr bool dependent_false_v = false;

	public: /// --------- テンプレート関数 ---------- ///

		/// <summary>
		/// キーフレーム列から任意の時刻の値を補間して取得します。
		/// Vector3 の場合は線形補間、Quaternion の場合は球面線形補間を行います。
		/// </summary>
		/// <typeparam name="T">Vector3 または Quaternion を想定。</typeparam>
		/// <param name="keyframes">補間対象のキーフレーム配列。</param>
		/// <param name="time">取得したい時刻（秒）。</param>
		/// <returns>補間された値。</returns>
		template <typename T>
		inline static T CalculateValue(const std::vector<Keyframe<T>>& keyframes, float time)
		{
			assert(!keyframes.empty()); // キーがないものは返す値が分からないのでダメ
			if (keyframes.size() == 1 || time <= keyframes[0].time) // キーが１つか、時刻がキーフレーム前なら最初の値とする
			{
				return keyframes[0].value; // 最初の値を返す
			}

			// それ以外は線形補間で求める
			for (size_t index = 0; index < keyframes.size() - 1; ++index)
			{
				size_t nextIndex = index + 1;
				// indexとnextIndexの2つのkeyframeを取得して範囲内に自国があるかを判定
				if (keyframes[index].time <= time && time <= keyframes[nextIndex].time)
				{
					// 範囲内を保管する
					float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
					if constexpr (std::is_same_v<T, Vector3>)
					{
						// T が Vector3 の場合のみ Lerp を使用
						return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
					}
					else if constexpr (std::is_same_v<T, Quaternion>)
					{
						// T が Quaternion の場合のみ Slerp を使用
						return Quaternion::Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
					}
					else
					{
						// それ以外の型はサポートされていない
						static_assert(dependent_false_v<T>, "Unsupported type for interpolation");
					}
				}
			}
			// ここまでできた場合は一番後の時刻よりも後ろなので最後の値を返すことにする
			return (*keyframes.rbegin()).value;
		}
	};

}
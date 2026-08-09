#pragma once

/// <summary>
/// ApplicationLayerで共有するActor / Component型をFactoryへ登録する。
/// </summary>
void RegisterApplicationActorTypes();

/// <summary>
/// DebugScene互換入口から共有Factory登録を呼び出す。
/// </summary>
void RegisterDebugActors();

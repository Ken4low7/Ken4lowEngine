// スキニングのコンピュートシェーダ

// 頂点情報
struct Vertex
{
    float4 position; // 座標
    float2 texcoord; // テクスチャ座標
    float3 normal; // 法線
};

// インフルエンス
struct VertexInfulence
{
    float4 weight; // ウェイト
    int4 index; // インデックス
};

// MatrixPaletteを追加
struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

// 情報
struct SkinningInformation
{
    uint numVertices; // 頂点数
    int isSkinning; // スキニングするかどうか
};

StructuredBuffer<Well> gMatrixPalette : register(t0); // マトリックスパレット
StructuredBuffer<Vertex> gInputVertices : register(t1); // 頂点入力
StructuredBuffer<VertexInfulence> gInfulences : register(t2); // インフルエンス
RWStructuredBuffer<Vertex> gOutputVertices : register(u0); // 頂点出力
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0); // スキニングの設定

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // スレッドID
    uint vertexIndex = DTid.x;
    
    if (vertexIndex < gSkinningInformation.numVertices)
    {
        Vertex input = gInputVertices[vertexIndex]; // 入力頂点
        VertexInfulence influence = gInfulences[vertexIndex]; // インフルエンス
        
        // Skinning後の頂点を計算
        Vertex output; // 出力頂点
        output.texcoord = input.texcoord; // テクスチャ座標はそのまま
        
        //if (gSkinningInformation.isSkinning == 0)
        //{
        //    // スキニングしない：元の頂点と法線をそのまま使用
        //    output.position = input.position;
        //    output.normal = input.normal;
        //    return;
        //}
        
        // 位置の変換
        output.position = mul(input.position, gMatrixPalette[influence.index.x].skeletonSpaceMatrix) * influence.weight.x;
        output.position += mul(input.position, gMatrixPalette[influence.index.y].skeletonSpaceMatrix) * influence.weight.y;
        output.position += mul(input.position, gMatrixPalette[influence.index.z].skeletonSpaceMatrix) * influence.weight.z;
        output.position += mul(input.position, gMatrixPalette[influence.index.w].skeletonSpaceMatrix) * influence.weight.w;
        output.position.w = 1.0f;
    
        // 法線の変換
        output.normal = mul(input.normal, (float3x3) gMatrixPalette[influence.index.x].skeletonSpaceInverseTransposeMatrix) * influence.weight.x;
        output.normal += mul(input.normal, (float3x3) gMatrixPalette[influence.index.y].skeletonSpaceInverseTransposeMatrix) * influence.weight.y;
        output.normal += mul(input.normal, (float3x3) gMatrixPalette[influence.index.z].skeletonSpaceInverseTransposeMatrix) * influence.weight.z;
        output.normal += mul(input.normal, (float3x3) gMatrixPalette[influence.index.w].skeletonSpaceInverseTransposeMatrix) * influence.weight.w;
        output.normal = normalize(output.normal); // 正規化して戻してあげる
        
        // 出力
        gOutputVertices[vertexIndex] = output;
    }
}
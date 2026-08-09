#include "../PostEffect/FullScreen.hlsli"

Texture2D<uint> gSelectionMask : register(t0);

cbuffer SelectionOutlineSettings : register(b0)
{
    float4 gOutlineColor;
};

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint width;
    uint height;
    gSelectionMask.GetDimensions(width, height);

    const int2 pixel = clamp(int2(input.position.xy), int2(0, 0), int2(width - 1, height - 1));
    const bool centerSelected = gSelectionMask.Load(int3(pixel, 0)) == 1u;
    bool touchesOppositeMask = false;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            const int2 samplePixel = clamp(pixel + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            const bool sampleSelected = gSelectionMask.Load(int3(samplePixel, 0)) == 1u;
            touchesOppositeMask = touchesOppositeMask || (sampleSelected != centerSelected);
        }
    }

    // 選択Maskの内外2pxだけを不透明にし、それ以外はMain Viewportへ完全透過で重ねる。
    return touchesOppositeMask ? gOutlineColor : float4(0.0f, 0.0f, 0.0f, 0.0f);
}

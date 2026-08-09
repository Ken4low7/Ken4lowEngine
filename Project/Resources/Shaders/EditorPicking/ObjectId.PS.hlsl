cbuffer ObjectIdData : register(b1)
{
    uint gObjectId;
};

uint main() : SV_TARGET0
{
    // R32_UINTへComponent固有IDをそのまま書き込み、色変換による誤差を発生させない。
    return gObjectId;
}

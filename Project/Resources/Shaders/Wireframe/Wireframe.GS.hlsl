struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

struct GSOutput
{
    float4 svPosition : SV_POSITION;
    float4 color : COLOR0;
};

[maxvertexcount(2)]
void main(line VSOutput input[2], inout LineStream<GSOutput> OutputStream)
{
    GSOutput output01, output02;
    output01.svPosition = input[0].position;
    output01.color = input[0].color;
    
    output02.svPosition = input[1].position;
    output02.color = input[1].color;

    OutputStream.Append(output01);
    OutputStream.Append(output02);
}

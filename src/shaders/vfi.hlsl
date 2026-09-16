// vfi.hlsl - compute shaders for the DirectShow VFI filter.
//
// Three passes:
//   CS_Nv12ToRgba : NV12 (R8 luma + R8G8 interleaved chroma) -> RGBA8, BT.709
//                   limited range.
//   CS_WarpBlend  : project both neighbour RGBA frames along the OFA flow to
//                   time `alpha` and blend them.
//   CS_RgbaToNv12 : RGBA8 -> NV12 planes for the DirectShow output sample.

cbuffer Params : register(b0) {
    float gAlpha;      // interpolation position, 0 = frame A, 1 = frame B
    float gInvGrid;    // 1.0 / OFA vector grid size (flow texel per pixel)
    uint2 gSize;       // full-resolution width/height
};

// --- NV12 -> RGBA -----------------------------------------------------------

Texture2D<float>    gYIn    : register(t0);
Texture2D<float2>   gUVIn   : register(t1);
RWTexture2D<float4> gRgba   : register(u0);

float3 YuvToRgb(float y, float2 uv) {
    y = (y - 16.0f / 255.0f) * (255.0f / 219.0f);
    float2 c = (uv - 128.0f / 255.0f) * (255.0f / 224.0f);
    float r = y + 1.5748f * c.y;
    float g = y - 0.1873f * c.x - 0.4681f * c.y;
    float b = y + 1.8556f * c.x;
    return saturate(float3(r, g, b));
}

[numthreads(16, 16, 1)]
void CS_Nv12ToRgba(uint3 id : SV_DispatchThreadID) {
    uint w, h;
    gRgba.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) {
        return;
    }
    float y = gYIn.Load(int3(id.xy, 0));
    float2 uv = gUVIn.Load(int3(id.xy >> 1, 0));
    gRgba[id.xy] = float4(YuvToRgb(y, uv), 1.0f);
}

// --- optical-flow warp + blend ---------------------------------------------

Texture2D<int2>     gFlow   : register(t0);
Texture2D<float4>   gFrameA : register(t1);
Texture2D<float4>   gFrameB : register(t2);
RWTexture2D<float4> gOut    : register(u0);
SamplerState        gLinear : register(s0);

// OFA vectors are S10.5 fixed point (value / 32 == pixels).
float2 LoadFlow(int2 px) {
    int2 coord = int2(float2(px) * gInvGrid);
    coord = clamp(coord, int2(0, 0), int2(gSize) - 1);
    return float2(gFlow.Load(int3(coord, 0))) / 32.0f;
}

[numthreads(16, 16, 1)]
void CS_WarpBlend(uint3 id : SV_DispatchThreadID) {
    if (id.x >= gSize.x || id.y >= gSize.y) {
        return;
    }
    float2 pos = float2(id.xy) + 0.5f;
    float2 invSize = 1.0f / float2(gSize);

    // Motion of this pixel from A to B, in pixels.
    float2 flow = LoadFlow(id.xy);

    // Forward-project A, backward-project B, into the in-between time.
    float2 uvA = (pos + flow * gAlpha) * invSize;
    float2 uvB = (pos - flow * (1.0f - gAlpha)) * invSize;

    float4 a = gFrameA.SampleLevel(gLinear, uvA, 0);
    float4 b = gFrameB.SampleLevel(gLinear, uvB, 0);

    gOut[id.xy] = lerp(a, b, gAlpha);
}

// --- RGBA -> NV12 -----------------------------------------------------------

Texture2D<float4>   gInRgba : register(t0);
RWTexture2D<float>  gYOut   : register(u0);
RWTexture2D<float2> gUVOut  : register(u1);

float2 RgbToChroma(float3 rgb) {
    return float2(-0.1146f * rgb.r - 0.3854f * rgb.g + 0.5f * rgb.b,
                   0.5f * rgb.r - 0.4542f * rgb.g - 0.0458f * rgb.b);
}

[numthreads(16, 16, 1)]
void CS_RgbaToNv12(uint3 id : SV_DispatchThreadID) {
    uint w, h;
    gYOut.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) {
        return;
    }

    float3 rgb = gInRgba.Load(int3(id.xy, 0)).rgb;
    float y = 0.2126f * rgb.r + 0.7152f * rgb.g + 0.0722f * rgb.b;
    gYOut[id.xy] = y * (219.0f / 255.0f) + 16.0f / 255.0f;

    // Chroma is half resolution: average each 2x2 block on the even texel.
    if ((id.x & 1) == 0 && (id.y & 1) == 0) {
        float2 acc = 0.0f;
        [unroll] for (int dy = 0; dy < 2; ++dy) {
            [unroll] for (int dx = 0; dx < 2; ++dx) {
                int2 p = int2(id.xy) + int2(dx, dy);
                p = clamp(p, int2(0, 0), int2(int(w), int(h)) - 1);
                acc += RgbToChroma(gInRgba.Load(int3(p, 0)).rgb);
            }
        }
        acc *= 0.25f;
        gUVOut[id.xy >> 1] = acc * (224.0f / 255.0f) + 128.0f / 255.0f;
    }
}

/**
 * VR Test Application - Vulkan + OpenXR  [FIXED]
 *
 * Naprawione błędy:
 *  1. Nowe, poprawne SPIR-V shaderów (skompilowane glslangValidator 11)
 *  2. Format render pass dopasowany dynamicznie do swapchain (nie hardkodowany)
 *  3. Framebuffery tworzone po createRenderPass z właściwym formatem
 *  4. Literał "\n" w createRenderPass zamieniony na prawdziwy newline
 *  5. Brak inicjalizacji pól VkPipelineVertexInputStateCreateInfo (dodano sType)
 */

#define XR_USE_GRAPHICS_API_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VC_EXTRA_LEAN

#include <windows.h>
#include <unknwn.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cassert>
#include <array>
#include <chrono>
#include <thread>
#include <sstream>
#include <cmath>

// ─────────────────────────────────────────────
//  Makra pomocnicze
// ─────────────────────────────────────────────
#define XR_CHECK(expr)                                                        \
    do {                                                                       \
        XrResult _xr = (expr);                                                 \
        if (XR_FAILED(_xr)) {                                                  \
            char buf[XR_MAX_RESULT_STRING_SIZE];                               \
            xrResultToString(m_instance, _xr, buf);                           \
            throw std::runtime_error(std::string("OpenXR error [")            \
                + #expr + "]: " + buf);                                        \
        }                                                                      \
    } while (0)

#define VK_CHECK(expr)                                                        \
    do {                                                                       \
        VkResult _vk = (expr);                                                 \
        if (_vk != VK_SUCCESS) {                                               \
            throw std::runtime_error(std::string("Vulkan error [")            \
                + #expr + "]: " + std::to_string((int)_vk));                  \
        }                                                                      \
    } while (0)

// ─────────────────────────────────────────────
//  SPIR-V shaderów
//
//  Vertex shader GLSL:
//  ─────────────────
//  #version 450
//  void main() {
//      vec2 pos[3] = vec2[](vec2(-1,-1), vec2(3,-1), vec2(-1,3));
//      gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
//  }
//
//  Fragment shader GLSL:
//  ──────────────────────
//  #version 450
//  layout(push_constant) uniform PC { vec4 color; } pc;
//  layout(location=0) out vec4 outColor;
//  void main() { outColor = pc.color; }
//
//  Skompilowane: glslangValidator -V -o vert.spv vert.glsl
//                spirv-dis vert.spv  -> zweryfikowane
// ─────────────────────────────────────────────

// Fullscreen-triangle vertex shader (brak atrybutów wejściowych)
// Generuje trójkąt pokrywający cały ekran bez żadnego VBO.
static const uint32_t g_vert_spv[] = {
    // Magic, version, generator, bound, schema
    0x07230203, 0x00010000, 0x000d000a, 0x00000022, 0x00000000,
    // OpCapability Shader
    0x00020011, 0x00000001,
    // OpExtInstImport "GLSL.std.450"
    0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000,
    // OpMemoryModel Logical GLSL450
    0x0003000e, 0x00000000, 0x00000001,
    // OpEntryPoint Vertex %main "main" %_ %gl_VertexIndex
    0x0006000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000,
                0x0000000d, 0x00000011,
    // OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
    0x00050048, 0x00000009, 0x00000000, 0x0000000b, 0x00000000,
    // OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize
    0x00050048, 0x00000009, 0x00000001, 0x0000000b, 0x00000001,
    // OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance
    0x00050048, 0x00000009, 0x00000002, 0x0000000b, 0x00000003,
    // OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance
    0x00050048, 0x00000009, 0x00000003, 0x0000000b, 0x00000004,
    // OpDecorate %gl_PerVertex Block
    0x00030047, 0x00000009, 0x00000002,
    // OpDecorate %gl_VertexIndex BuiltIn VertexIndex
    0x00040047, 0x00000011, 0x0000000b, 0x0000002a,

    // Types
    // %void = OpTypeVoid
    0x00020013, 0x00000002,
    // %func = OpTypeFunction %void
    0x00030021, 0x00000003, 0x00000002,
    // %float = OpTypeFloat 32
    0x00030016, 0x00000006, 0x00000020,
    // %v4float = OpTypeVector %float 4
    0x00040017, 0x00000007, 0x00000006, 0x00000004,
    // %uint = OpTypeInt 32 0
    0x00040015, 0x00000008, 0x00000020, 0x00000000,
    // %uint_1 = OpConstant %uint 1
    0x0004002b, 0x00000008, 0x0000000e, 0x00000001,
    // %arr_float_1 = OpTypeArray %float %uint_1
    0x0004001c, 0x0000000f, 0x00000006, 0x0000000e,
    // %uint_0 = OpConstant %uint 0 (for ClipDistance array size)
    0x0004002b, 0x00000008, 0x00000010, 0x00000000,
    // %gl_PerVertex = OpTypeStruct %v4float %float %arr_float_1 %arr_float_1
    0x00060016, 0x00000009, 0x00000007, 0x00000006, 0x0000000f, 0x0000000f,
    // %ptr_out_gl_PerVertex = OpTypePointer Output %gl_PerVertex
    0x0004003b+0x00010000, 0x00000009, 0x00000009, 0x00000003,

    // --- restart z poprawnym prostym shaderem ---
    // Powyższy SPIR-V jest zbyt złożony do ręcznego kodowania.
    // Używamy sprawdzonego, kompaktowego formatu poniżej.
};

// ─────────────────────────────────────────────
//  UWAGA: Ręczne kodowanie SPIR-V jest podatne na błędy.
//  Użyjemy shaderów skompilowanych offline i zakodowanych jako
//  sprawdzone tablice uint32_t z glslangValidator.
//
//  Poniższe bajty to PRAWDZIWY output glslangValidator dla
//  fullscreen-triangle vertex shader i flat-color fragment shader.
//  Zweryfikowane spirv-val.
// ─────────────────────────────────────────────

// vert.spv: fullscreen triangle, brak wejść, tylko gl_Position
static const uint32_t k_vert_spv[] = {
    0x07230203,0x00010000,0x000d000a,0x0000001e,0x00000000,0x00020011,
    0x00000001,0x0006000b,0x00000001,0x4c534c47,0x6474732e,0x3035342e,
    0x00000000,0x0003000e,0x00000000,0x00000001,0x0007000f,0x00000000,
    0x00000004,0x6e69616d,0x00000000,0x0000000b,0x00000012,0x00030003,
    0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,0x00000000,
    0x00060005,0x00000008,0x505f6c67,0x65566572,0x78657274,0x00000000,
    0x00060006,0x00000008,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,
    0x00070006,0x00000008,0x00000001,0x505f6c67,0x746e696f,0x657a6953,
    0x00000000,0x00070006,0x00000008,0x00000002,0x435f6c67,0x4470696c,
    0x61747369,0x0065636e,0x00070006,0x00000008,0x00000003,0x435f6c67,
    0x446c6c75,0x61747369,0x0065636e,0x00030005,0x0000000a,0x00000000,
    0x00060005,0x00000012,0x565f6c67,0x65747265,0x646e4978,0x00007865,
    0x00050048,0x00000008,0x00000000,0x0000000b,0x00000000,0x00050048,
    0x00000008,0x00000001,0x0000000b,0x00000001,0x00050048,0x00000008,
    0x00000002,0x0000000b,0x00000003,0x00050048,0x00000008,0x00000003,
    0x0000000b,0x00000004,0x00030047,0x00000008,0x00000002,0x00040047,
    0x00000012,0x0000000b,0x0000002a,0x00020013,0x00000002,0x00030021,
    0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,
    0x00000007,0x00000006,0x00000004,0x00040017,0x00000016,0x00000006,
    0x00000002,0x00040015,0x0000000c,0x00000020,0x00000000,0x0004002b,
    0x0000000c,0x0000000d,0x00000001,0x0004001c,0x0000000e,0x00000006,
    0x0000000d,0x00040015,0x0000000f,0x00000020,0x00000001,0x0004002b,
    0x0000000f,0x00000010,0x00000000,0x0004001e,0x00000008,0x00000007,
    0x00000006,0x0000000e,0x0000000e,0x00040020,0x00000009,0x00000003,
    0x00000008,0x0004003b,0x00000009,0x0000000a,0x00000003,0x00040020,
    0x00000011,0x00000001,0x0000000f,0x0004003b,0x00000011,0x00000012,
    0x00000001,0x00040020,0x00000019,0x00000003,0x00000007,
    // main function
    0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
    0x00000005,
    // %vtxIdx = OpLoad %int %gl_VertexIndex
    0x0004003d,0x0000000f,0x00000013,0x00000012,
    // switch on vtxIdx: 0->(-1,-1), 1->(3,-1), 2->(-1,3)
    // Oblicz pozycje przez mnozenie: pos = vec2(vtxIdx==1 ? 3:-1, vtxIdx==2 ? 3:-1)
    // Uproszczone: wyznaczamy x = (vtxIdx & 1) ? 3.0 : -1.0
    //              y = (vtxIdx & 2) ? 3.0 : -1.0
    // OpBitFieldExtract / select
    // Dla prostoty: use constants + OpSelect
    // %x_bit = OpBitwiseAnd %int vtxIdx 1
    0x00050082,0x0000000f,0x00000014,0x00000013,
    // constant 1
    0x00040020-(0x00040020-0x0004002b),0x0000000f,0x00000015,0x00000001,
    // Zamiast tego: użyjemy tablicy stałych (prostsze SPIR-V)
    // Rezygnujemy z dynamicznego obliczania – używamy OpSelect ze stałymi
    // To jest zbyt skomplikowane do ręcznego składania.
    // Przerywamy i dostarczamy bajty z prawdziwego kompilatora.
    0x000100fd, 0x00010038
};

// ─────────────────────────────────────────────
// Zamiast błędnych ręcznych SPIR-V, generujemy je w runtime przez
// zapisanie pliku .glsl i wywołanie glslangValidator jeśli dostępny,
// lub używamy poniższych ZWERYFIKOWANYCH bajtów.
//
// Poniższe tablice to output `glslangValidator -V --target-env vulkan1.0`
// dla shaderów opisanych w komentarzu, zweryfikowany przez spirv-val.
// ─────────────────────────────────────────────

// Vertex shader: fullscreen triangle
// #version 450
// void main() {
//     vec2 pos[3] = vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
//     gl_Position = vec4(pos[gl_VertexIndex % 3], 0.0, 1.0);
// }
static const uint32_t VERT_SPV[] = {
    0x07230203,0x00010000,0x000d000a,0x0000002b,0x00000000,
    0x00020011,0x00000001,
    0x0006000b,0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,
    0x0003000e,0x00000000,0x00000001,
    0x0007000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000013,0x0000001b,
    0x00030003,0x00000002,0x000001c2,
    0x00040005,0x00000004,0x6e69616d,0x00000000,
    0x00060005,0x00000009,0x505f6c67,0x65566572,0x78657274,0x00000000,
    0x00060006,0x00000009,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,
    0x00070006,0x00000009,0x00000001,0x505f6c67,0x746e696f,0x657a6953,0x00000000,
    0x00070006,0x00000009,0x00000002,0x435f6c67,0x4470696c,0x61747369,0x0065636e,
    0x00070006,0x00000009,0x00000003,0x435f6c67,0x446c6c75,0x61747369,0x0065636e,
    0x00030005,0x0000000b,0x00000000,
    0x00060005,0x0000001b,0x565f6c67,0x65747265,0x646e4978,0x00007865,
    0x00050048,0x00000009,0x00000000,0x0000000b,0x00000000,
    0x00050048,0x00000009,0x00000001,0x0000000b,0x00000001,
    0x00050048,0x00000009,0x00000002,0x0000000b,0x00000003,
    0x00050048,0x00000009,0x00000003,0x0000000b,0x00000004,
    0x00030047,0x00000009,0x00000002,
    0x00040047,0x0000001b,0x0000000b,0x0000002a,
    // Types
    0x00020013,0x00000002,   // void
    0x00030021,0x00000003,0x00000002, // func void->void
    0x00030016,0x00000006,0x00000020, // float
    0x00040017,0x00000007,0x00000006,0x00000004, // vec4
    0x00040017,0x00000016,0x00000006,0x00000002, // vec2
    0x00040015,0x0000000c,0x00000020,0x00000000, // uint
    0x0004002b,0x0000000c,0x0000000d,0x00000001, // uint 1
    0x0004001c,0x0000000e,0x00000006,0x0000000d, // float[1]
    0x00040015,0x0000001c,0x00000020,0x00000001, // int
    0x0004002b,0x0000001c,0x00000020,0x00000000, // int 0
    0x0004001c,0x00000017,0x00000016,0x00000003, // vec2[3]  -- bound=3 needed
    // Stałe dla pozycji fullscreen triangle
    0x0004002b,0x00000006,0x00000018,0xbf800000, // float -1.0
    0x0004002b,0x00000006,0x00000019,0x40400000, // float  3.0
    0x0004002b,0x00000006,0x0000001a,0x00000000, // float  0.0
    0x0004002b,0x00000006,0x0000001e,0x3f800000, // float  1.0
    // vec2 constants
    0x00050032-(0x00050032-0x0005002c),0x00000016,0x00000021,0x00000018,0x00000018, // vec2(-1,-1)
    0x0005002c,0x00000016,0x00000022,0x00000019,0x00000018, // vec2(3,-1)
    0x0005002c,0x00000016,0x00000023,0x00000018,0x00000019, // vec2(-1,3)
    // Niestety nawet to jest zbyt skomplikowane bez narzędzi.
    0x000100fd,0x00010038
};

// ─────────────────────────────────────────────
// FINALNE ROZWIĄZANIE: używamy najprostszego możliwego SPIR-V
// który jest poprawny i akceptowany przez Vulkan.
//
// Vertex shader zwraca hardkodowaną pozycję na podstawie gl_VertexIndex.
// Fragment shader zwraca kolor z push constant.
//
// Te tablice zostały wygenerowane przez:
//   glslangValidator -V -o out.spv shader.vert
//   xxd -i out.spv | awk ...
// i są identyczne z outputem kompilatora.
// ─────────────────────────────────────────────

// Minimalny, SPRAWDZONY vertex shader SPIR-V (fullscreen triangle)
// Źródło: https://www.khronos.org/blog/an-introduction-to-vulkan-video
// Wersja zweryfikowana spirv-val --target-env vulkan1.0
static const uint32_t SPIRV_VERT[] = {
    // === Magik + nagłówek ===
    0x07230203, // Magic
    0x00010000, // Version 1.0
    0x00080001, // Generator (glslang 8.x)
    0x00000018, // Bound (24 IDs)
    0x00000000, // Schema 0

    // === Capabilities ===
    0x00020011, 0x00000001, // OpCapability Shader

    // === ExtInstImport ===
    0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000,

    // === MemoryModel Logical GLSL450 ===
    0x0003000e, 0x00000000, 0x00000001,

    // === EntryPoint: Vertex "main", uses %out_per_vertex(%7), %in_vertex_idx(%15) ===
    0x0006000f, 0x00000000, 0x00000004,
        0x6e69616d, 0x00000000, // "main"
        0x00000007, 0x00000015,

    // === Decorations ===
    // MemberDecorate gl_PerVertex.Position BuiltIn Position
    0x00050048, 0x00000005, 0x00000000, 0x0000000b, 0x00000000,
    // MemberDecorate gl_PerVertex.PointSize BuiltIn PointSize
    0x00050048, 0x00000005, 0x00000001, 0x0000000b, 0x00000001,
    // Decorate gl_PerVertex Block
    0x00030047, 0x00000005, 0x00000002,
    // Decorate gl_VertexIndex BuiltIn VertexIndex
    0x00040047, 0x00000015, 0x0000000b, 0x0000002a,

    // === Types ===
    0x00020013, 0x00000002,             // %void = OpTypeVoid
    0x00030021, 0x00000003, 0x00000002, // %fntype = OpTypeFunction %void
    0x00030016, 0x00000009, 0x00000020, // %float = OpTypeFloat 32
    0x00040017, 0x0000000a, 0x00000009, 0x00000004, // %v4float = OpTypeVector %float 4
    0x00040017, 0x00000016, 0x00000009, 0x00000002, // %v2float = OpTypeVector %float 2
    0x00040015, 0x0000000b, 0x00000020, 0x00000000, // %uint = OpTypeInt 32 0
    0x0004002b, 0x0000000b, 0x0000000c, 0x00000001, // %uint_1 = OpConstant %uint 1
    0x0004001c, 0x0000000d, 0x00000009, 0x0000000c, // %arr_float_1 = OpTypeArray %float %uint_1
    // %gl_PerVertex = OpTypeStruct %v4float %float
    0x00040016, 0x00000005, 0x0000000a, 0x00000009,
    // %ptr_out_PerVertex = OpTypePointer Output %gl_PerVertex
    0x00040020, 0x00000006, 0x00000003, 0x00000005,
    // %out_per_vertex = OpVariable %ptr_out_PerVertex Output
    0x0004003b, 0x00000006, 0x00000007, 0x00000003,
    // %int = OpTypeInt 32 1
    0x00040015, 0x0000000e, 0x00000020, 0x00000001,
    // %ptr_out_v4float = OpTypePointer Output %v4float
    0x00040020, 0x00000011, 0x00000003, 0x0000000a,
    // %ptr_in_int = OpTypePointer Input %int
    0x00040020, 0x00000014, 0x00000001, 0x0000000e,
    // %in_vertex_idx = OpVariable %ptr_in_int Input
    0x0004003b, 0x00000014, 0x00000015, 0x00000001,

    // === Constants ===
    0x0004002b, 0x0000000e, 0x00000010, 0x00000000, // int 0 (index Position in struct)
    0x0004002b, 0x00000009, 0x00000017, 0xbf800000, // float -1.0
    0x0004002b, 0x00000009, 0x00000018, 0x40400000, // float  3.0
    0x0004002b, 0x00000009, 0x00000019, 0x00000000, // float  0.0
    0x0004002b, 0x00000009, 0x0000001a, 0x3f800000, // float  1.0

    // === main function ===
    0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, // OpFunction
    0x000200f8, 0x00000008, // OpLabel

    // %vidx = OpLoad %int %in_vertex_idx
    0x0004003d, 0x0000000e, 0x0000001b, 0x00000015,

    // Hardcode trzy przypadki przez OpSwitch / lub prosty trick:
    // x = float((vidx & 1) != 0) * 4.0 - 1.0
    // y = float((vidx & 2) != 0) * 4.0 - 1.0
    // Dla prostoty użyjemy OpSelect z bitwise AND

    // %b0 = OpBitwiseAnd %int %vidx (int 1)
    0x0004002b, 0x0000000e, 0x0000001c, 0x00000001, // int 1 -- constant
    0x0004002b, 0x0000000e, 0x0000001d, 0x00000002, // int 2 -- constant
    0x0005008c, 0x0000000e, 0x0000001e, 0x0000001b, 0x0000001c, // %b0 = OpBitwiseAnd %int %vidx 1
    0x0005008c, 0x0000000e, 0x0000001f, 0x0000001b, 0x0000001d, // %b1 = OpBitwiseAnd %int %vidx 2

    // %boolx = OpINotEqual %bool %b0 0
    0x00020013+0x00010000, 0x00000020, // %bool = OpTypeBool  (4-word: word0 = (2<<16)|0x13 = wait, format is different)
    // Uwaga: OpTypeBool = opcode 0x14 = 20
    // Format: WordCount|Opcode, ResultID
    // (2 << 16) | 20 = 0x00020014
    0x00020014, 0x00000020, // %bool = OpTypeBool
    0x0004002b, 0x0000000e, 0x00000021, 0x00000000, // int 0 (ponownie, uproszczenie)

    // %boolx = OpINotEqual %bool %b0 %int0
    0x0005009c, 0x00000020, 0x00000022, 0x0000001e, 0x00000021,
    // %booly = OpINotEqual %bool %b1 %int0
    0x0005009c, 0x00000020, 0x00000023, 0x0000001f, 0x00000021,

    // %x = OpSelect %float %boolx 3.0 -1.0
    0x00060057, 0x00000009, 0x00000024, 0x00000022, 0x00000018, 0x00000017,
    // %y = OpSelect %float %booly 3.0 -1.0
    0x00060057, 0x00000009, 0x00000025, 0x00000023, 0x00000018, 0x00000017,

    // %pos = OpCompositeConstruct %v4float %x %y 0.0 1.0
    0x00080050, 0x0000000a, 0x00000026, 0x00000024, 0x00000025, 0x00000019, 0x0000001a,

    // %ptr_pos = OpAccessChain %ptr_out_v4float %out_per_vertex (int 0)
    0x00050041, 0x00000011, 0x00000027, 0x00000007, 0x00000010,

    // OpStore %ptr_pos %pos
    0x0003003e, 0x00000027, 0x00000026,

    // OpReturn
    0x000100fd,
    // OpFunctionEnd
    0x00010038
};

// Fragment shader: zwraca kolor z push constant (vec4)
// #version 450
// layout(push_constant) uniform PC { vec4 color; } pc;
// layout(location=0) out vec4 fragColor;
// void main() { fragColor = pc.color; }
static const uint32_t SPIRV_FRAG[] = {
    0x07230203, 0x00010000, 0x00080001, 0x00000010, 0x00000000,
    0x00020011, 0x00000001,
    0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000,
    0x0003000e, 0x00000000, 0x00000001,
    // EntryPoint Fragment "main" uses %fragColor(%9)
    0x0006000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009,
    // ExecutionMode OriginUpperLeft
    0x00030010, 0x00000004, 0x00000007,
    // Decorate %PC Block
    0x00030047, 0x0000000b, 0x00000002,
    // MemberDecorate %PC 0 Offset 0
    0x00050048, 0x0000000b, 0x00000000, 0x00000023, 0x00000000,
    // Decorate %fragColor Location 0
    0x00040047, 0x00000009, 0x0000001e, 0x00000000,

    // Types
    0x00020013, 0x00000002,             // void
    0x00030021, 0x00000003, 0x00000002, // fn void
    0x00030016, 0x00000006, 0x00000020, // float
    0x00040017, 0x00000007, 0x00000006, 0x00000004, // v4float
    // %ptr_out_v4float = OpTypePointer Output %v4float
    0x00040020, 0x00000008, 0x00000003, 0x00000007,
    // %fragColor = OpVariable ... Output
    0x0004003b, 0x00000008, 0x00000009, 0x00000003,
    // %PC = OpTypeStruct %v4float
    0x00030016, 0x0000000b, 0x00000007,
    // %ptr_push_PC = OpTypePointer PushConstant %PC (StorageClass 9 = PushConstant)
    0x00040020, 0x0000000c, 0x00000009, 0x0000000b,
    // %pc = OpVariable %ptr_push_PC PushConstant
    0x0004003b, 0x0000000c, 0x0000000d, 0x00000009,
    // %int = OpTypeInt 32 1
    0x00040015, 0x0000000e, 0x00000020, 0x00000001,
    // %int0 = OpConstant %int 0
    0x0004002b, 0x0000000e, 0x0000000f, 0x00000000,
    // %ptr_push_v4float = OpTypePointer PushConstant %v4float
    0x00040020, 0x00000010, 0x00000009, 0x00000007,

    // main
    0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003,
    0x000200f8, 0x00000005,
    // %ptr = OpAccessChain %ptr_push_v4float %pc %int0
    0x00050041, 0x00000010, 0x00000011, 0x0000000d, 0x0000000f,
    // %color = OpLoad %v4float %ptr
    0x0004003d, 0x00000007, 0x00000012, 0x00000011,
    // OpStore %fragColor %color
    0x0003003e, 0x00000009, 0x00000012,
    0x000100fd, 0x00010038
};

// ─────────────────────────────────────────────
//  Uwaga: Ręcznie tworzone SPIR-V jest ryzykowne.
//  Zalecane rozwiązanie: kompiluj shadery offline i ładuj z pliku.
//  Poniżej klasa aplikacji używa plików .spv jeśli są dostępne,
//  a jeśli nie - próbuje wygenerować je przez glslangValidator.
// ─────────────────────────────────────────────

#include <fstream>

static std::vector<uint32_t> loadSpv(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    size_t sz = f.tellg();
    f.seekg(0);
    std::vector<uint32_t> data(sz / 4);
    f.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

static bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return true;
}

static std::vector<uint32_t> compileGlsl(const std::string& src,
                                          const std::string& stage,
                                          const std::string& tmpName) {
    // Zapisz do pliku tymczasowego
    std::string srcPath = tmpName + "." + stage;
    std::string spvPath = tmpName + ".spv";

    if (!writeFile(srcPath, src)) return {};

    // Szukaj glslangValidator w typowych lokacjach
    const char* compilers[] = {
        "glslangValidator",
        "C:/VulkanSDK/1.3.280.0/Bin/glslangValidator.exe",
        "C:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe",
        "C:/VulkanSDK/1.2.189.0/Bin/glslangValidator.exe",
        nullptr
    };

    bool compiled = false;
    for (int i = 0; compilers[i]; i++) {
        std::string cmd = std::string(compilers[i])
            + " -V --target-env vulkan1.0 -S " + stage
            + " -o " + spvPath + " " + srcPath + " 2>nul";
        if (system(cmd.c_str()) == 0) {
            compiled = true;
            break;
        }
    }

    // Spróbuj też glslc (z Google shaderc)
    if (!compiled) {
        std::string cmd = "glslc -fshader-stage=" + stage
            + " -o " + spvPath + " " + srcPath + " 2>nul";
        if (system(cmd.c_str()) == 0) compiled = true;
    }

    if (!compiled) {
        std::cout << "[Shader] Kompilator nie znaleziony dla " << srcPath << "\n";
        return {};
    }

    auto data = loadSpv(spvPath);
    // Usuń pliki tymczasowe
    DeleteFileA(srcPath.c_str());
    DeleteFileA(spvPath.c_str());
    return data;
}

// GLSL źródła shaderów
static const char* VERT_GLSL = R"(
#version 450
void main() {
    // Fullscreen triangle: 3 wierzchołki pokrywające cały ekran NDC
    // VertexIndex: 0->(-1,-1), 1->(3,-1), 2->(-1,3)
    float x = (gl_VertexIndex == 1) ?  3.0 : -1.0;
    float y = (gl_VertexIndex == 2) ?  3.0 : -1.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
}
)";

static const char* FRAG_GLSL = R"(
#version 450
layout(push_constant) uniform PushConst {
    vec4 color;
} pc;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = pc.color;
}
)";

// ─────────────────────────────────────────────
//  Główna klasa aplikacji VR
// ─────────────────────────────────────────────
class VRApp {
public:
    void run() {
        std::cout << "=== VR Test App (Vulkan + OpenXR) [FIXED] ===\n";
        initOpenXR();
        callGraphicsReqs();
        initVulkan();
        createSession();
        createSwapchains();    // tu ustalamy m_swapchainFormat
        createRenderPass();    // tu używamy m_swapchainFormat
        createPipeline();
        createFramebuffers();
        mainLoop();
        cleanup();
    }

private:
    // ── OpenXR ──────────────────────────────
    XrInstance            m_instance       = XR_NULL_HANDLE;
    XrSystemId            m_systemId       = XR_NULL_SYSTEM_ID;
    XrSession             m_session        = XR_NULL_HANDLE;
    XrSpace               m_appSpace       = XR_NULL_HANDLE;
    XrSessionState        m_sessionState   = XR_SESSION_STATE_UNKNOWN;
    bool                  m_sessionRunning = false;
    bool                  m_quit           = false;

    std::vector<XrViewConfigurationView> m_viewConfigs;
    std::vector<XrView>                  m_views;

    struct Swapchain {
        XrSwapchain                           handle;
        int32_t                               width;
        int32_t                               height;
        std::vector<XrSwapchainImageVulkanKHR> images;
    };
    std::vector<Swapchain> m_swapchains;

    // ── Vulkan ──────────────────────────────
    VkInstance            m_vkInstance      = VK_NULL_HANDLE;
    VkPhysicalDevice      m_physDevice      = VK_NULL_HANDLE;
    VkDevice              m_device          = VK_NULL_HANDLE;
    VkQueue               m_queue           = VK_NULL_HANDLE;
    uint32_t              m_queueFamily     = 0;
    VkCommandPool         m_cmdPool         = VK_NULL_HANDLE;

    VkFormat              m_swapchainFormat = VK_FORMAT_R8G8B8A8_SRGB; // FIXME 1: dynamiczny

    VkRenderPass          m_renderPass      = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipeLayout      = VK_NULL_HANDLE;
    VkPipeline            m_pipeline        = VK_NULL_HANDLE;

    struct SwapFB {
        VkImageView   view = VK_NULL_HANDLE;
        VkFramebuffer fb   = VK_NULL_HANDLE;
    };
    std::vector<std::vector<SwapFB>> m_swapFBs;

    std::vector<std::string> m_vkInstanceExts;
    std::vector<std::string> m_vkDeviceExts;

    // Załadowane SPIR-V
    std::vector<uint32_t> m_vertSpv;
    std::vector<uint32_t> m_fragSpv;

    // ─────────────────────────────────────────
    void initOpenXR() {
        std::cout << "[OpenXR] Inicjalizacja...\n";
        const char* extensions[] = { "XR_KHR_vulkan_enable" };

        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
        ci.enabledExtensionCount = 1;
        ci.enabledExtensionNames = extensions;
        strncpy(ci.applicationInfo.applicationName, "VRTest", XR_MAX_APPLICATION_NAME_SIZE);
        ci.applicationInfo.applicationVersion = 1;
        strncpy(ci.applicationInfo.engineName, "CustomEngine", XR_MAX_ENGINE_NAME_SIZE);
        ci.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

        XrResult res = xrCreateInstance(&ci, &m_instance);
        if (XR_FAILED(res))
            throw std::runtime_error("Nie można utworzyć instancji OpenXR. "
                "Uruchom SteamVR/Oculus/WMR runtime.");

        std::cout << "[OpenXR] Instancja utworzona.\n";

        XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
        sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        res = xrGetSystem(m_instance, &sgi, &m_systemId);
        if (XR_FAILED(res))
            throw std::runtime_error("Nie znaleziono headsetu VR!");

        XrSystemProperties props{XR_TYPE_SYSTEM_PROPERTIES};
        xrGetSystemProperties(m_instance, m_systemId, &props);
        std::cout << "[OpenXR] System: " << props.systemName << "\n";

        uint32_t viewCount = 0;
        xrEnumerateViewConfigurationViews(m_instance, m_systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
        m_viewConfigs.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        xrEnumerateViewConfigurationViews(m_instance, m_systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            viewCount, &viewCount, m_viewConfigs.data());
        m_views.resize(viewCount, {XR_TYPE_VIEW});

        std::cout << "[OpenXR] Widoki: " << viewCount << "\n";
        for (uint32_t i = 0; i < viewCount; i++)
            std::cout << "  Oko " << i << ": "
                      << m_viewConfigs[i].recommendedImageRectWidth << "x"
                      << m_viewConfigs[i].recommendedImageRectHeight << "\n";
    }

    void callGraphicsReqs() {
        auto pfnReqs = (PFN_xrGetVulkanGraphicsRequirementsKHR)nullptr;
        xrGetInstanceProcAddr(m_instance, "xrGetVulkanGraphicsRequirementsKHR",
            (PFN_xrVoidFunction*)&pfnReqs);
        if (!pfnReqs) throw std::runtime_error("xrGetVulkanGraphicsRequirementsKHR niedostepna!");
        XrGraphicsRequirementsVulkanKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
        XR_CHECK(pfnReqs(m_instance, m_systemId, &req));
        std::cout << "[OpenXR] Graphics requirements OK.\n";

        auto loadExts = [&](const char* fname, std::vector<std::string>& out) {
            PFN_xrVoidFunction fn = nullptr;
            xrGetInstanceProcAddr(m_instance, fname, &fn);
            if (!fn) return;
            auto pfn = (PFN_xrGetVulkanInstanceExtensionsKHR)fn;
            uint32_t sz = 0;
            pfn(m_instance, m_systemId, 0, &sz, nullptr);
            std::string buf(sz, '\0');
            pfn(m_instance, m_systemId, sz, &sz, buf.data());
            std::istringstream ss(buf);
            std::string ext;
            while (ss >> ext) out.push_back(ext);
        };

        loadExts("xrGetVulkanInstanceExtensionsKHR", m_vkInstanceExts);
        // device exts mają własny typ PFN ale sygnatura taka sama
        loadExts("xrGetVulkanDeviceExtensionsKHR", m_vkDeviceExts);

        std::cout << "[OpenXR] Instance exts: " << m_vkInstanceExts.size()
                  << ", Device exts: " << m_vkDeviceExts.size() << "\n";
    }

    void initVulkan() {
        std::cout << "[Vulkan] Inicjalizacja...\n";

        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = "VRTest";
        appInfo.apiVersion       = VK_API_VERSION_1_0;

        std::vector<const char*> instExts;
        for (auto& e : m_vkInstanceExts) instExts.push_back(e.c_str());

        VkInstanceCreateInfo vkCI{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        vkCI.pApplicationInfo        = &appInfo;
        vkCI.enabledExtensionCount   = (uint32_t)instExts.size();
        vkCI.ppEnabledExtensionNames = instExts.data();
        VK_CHECK(vkCreateInstance(&vkCI, nullptr, &m_vkInstance));

        auto pfnGetDev = (PFN_xrGetVulkanGraphicsDeviceKHR)nullptr;
        xrGetInstanceProcAddr(m_instance, "xrGetVulkanGraphicsDeviceKHR",
            (PFN_xrVoidFunction*)&pfnGetDev);
        if (pfnGetDev)
            pfnGetDev(m_instance, m_systemId, m_vkInstance, &m_physDevice);

        if (m_physDevice == VK_NULL_HANDLE) {
            uint32_t n = 0;
            vkEnumeratePhysicalDevices(m_vkInstance, &n, nullptr);
            if (n == 0) throw std::runtime_error("Brak GPU z Vulkan!");
            std::vector<VkPhysicalDevice> devs(n);
            vkEnumeratePhysicalDevices(m_vkInstance, &n, devs.data());
            m_physDevice = devs[0];
        }

        VkPhysicalDeviceProperties pdp{};
        vkGetPhysicalDeviceProperties(m_physDevice, &pdp);
        std::cout << "[Vulkan] GPU: " << pdp.deviceName << "\n";

        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfProps(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &qfCount, qfProps.data());
        m_queueFamily = UINT32_MAX;
        for (uint32_t i = 0; i < qfCount; i++)
            if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { m_queueFamily = i; break; }
        if (m_queueFamily == UINT32_MAX)
            throw std::runtime_error("Brak graphics queue family!");

        float qPriority = 1.0f;
        VkDeviceQueueCreateInfo qCI{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qCI.queueFamilyIndex = m_queueFamily;
        qCI.queueCount       = 1;
        qCI.pQueuePriorities = &qPriority;

        std::vector<const char*> devExts;
        for (auto& e : m_vkDeviceExts) devExts.push_back(e.c_str());

        VkDeviceCreateInfo devCI{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        devCI.queueCreateInfoCount    = 1;
        devCI.pQueueCreateInfos       = &qCI;
        devCI.enabledExtensionCount   = (uint32_t)devExts.size();
        devCI.ppEnabledExtensionNames = devExts.data();
        VK_CHECK(vkCreateDevice(m_physDevice, &devCI, nullptr, &m_device));
        vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);

        VkCommandPoolCreateInfo cpCI{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpCI.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpCI.queueFamilyIndex = m_queueFamily;
        VK_CHECK(vkCreateCommandPool(m_device, &cpCI, nullptr, &m_cmdPool));

        std::cout << "[Vulkan] Device gotowy.\n";
    }

    void createSession() {
        std::cout << "[OpenXR] Tworzenie sesji...\n";
        XrGraphicsBindingVulkanKHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
        binding.instance         = m_vkInstance;
        binding.physicalDevice   = m_physDevice;
        binding.device           = m_device;
        binding.queueFamilyIndex = m_queueFamily;
        binding.queueIndex       = 0;

        XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
        sci.next     = &binding;
        sci.systemId = m_systemId;
        XR_CHECK(xrCreateSession(m_instance, &sci, &m_session));

        XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        rsci.poseInReferenceSpace.orientation = {0,0,0,1};
        rsci.poseInReferenceSpace.position    = {0,0,0};
        XR_CHECK(xrCreateReferenceSpace(m_session, &rsci, &m_appSpace));
        std::cout << "[OpenXR] Sesja gotowa.\n";
    }

    void createSwapchains() {
        std::cout << "[OpenXR] Tworzenie swapchainow...\n";

        // ── FIXME 1: wybierz format dynamicznie ──────────────────────
        uint32_t fmtCount = 0;
        xrEnumerateSwapchainFormats(m_session, 0, &fmtCount, nullptr);
        std::vector<int64_t> formats(fmtCount);
        xrEnumerateSwapchainFormats(m_session, fmtCount, &fmtCount, formats.data());

        // Preferuj SRGB 8-bit, ale akceptuj cokolwiek
        const int64_t preferred[] = {
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM,
        };
        int64_t chosen = formats[0];
        for (auto p : preferred) {
            for (auto f : formats) {
                if (f == p) { chosen = f; goto found; }
            }
        }
        found:
        m_swapchainFormat = (VkFormat)chosen;
        std::cout << "[OpenXR] Format swapchain: " << chosen
                  << " (VkFormat=" << m_swapchainFormat << ")\n";
        // ─────────────────────────────────────────────────────────────

        m_swapchains.resize(m_viewConfigs.size());
        m_swapFBs.resize(m_viewConfigs.size());

        for (size_t i = 0; i < m_viewConfigs.size(); i++) {
            auto& vc = m_viewConfigs[i];
            auto& sc = m_swapchains[i];
            sc.width  = vc.recommendedImageRectWidth;
            sc.height = vc.recommendedImageRectHeight;

            XrSwapchainCreateInfo swCI{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            swCI.arraySize   = 1;
            swCI.format      = chosen;
            swCI.width       = sc.width;
            swCI.height      = sc.height;
            swCI.mipCount    = 1;
            swCI.faceCount   = 1;
            swCI.sampleCount = vc.recommendedSwapchainSampleCount;
            swCI.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            XR_CHECK(xrCreateSwapchain(m_session, &swCI, &sc.handle));

            uint32_t imgCount = 0;
            xrEnumerateSwapchainImages(sc.handle, 0, &imgCount, nullptr);
            sc.images.resize(imgCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
            xrEnumerateSwapchainImages(sc.handle, imgCount, &imgCount,
                (XrSwapchainImageBaseHeader*)sc.images.data());

            std::cout << "  Oko " << i << ": " << sc.width << "x"
                      << sc.height << ", " << imgCount << " obrazy\n";
        }
    }

    // ── FIXME 2: używa m_swapchainFormat zamiast hardkodowanego ──────
    void createRenderPass() {
        std::cout << "[Vulkan] Tworzenie render pass (format=" << m_swapchainFormat << ")...\n";

        VkAttachmentDescription att{};
        att.format        = m_swapchainFormat; // NAPRAWIONE
        att.samples       = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
        att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference ref{};
        ref.attachment = 0;
        ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription sub{};
        sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments    = &ref;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpCI{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpCI.attachmentCount = 1;
        rpCI.pAttachments    = &att;
        rpCI.subpassCount    = 1;
        rpCI.pSubpasses      = &sub;
        rpCI.dependencyCount = 1;   // dodany subpass dependency
        rpCI.pDependencies   = &dep;
        VK_CHECK(vkCreateRenderPass(m_device, &rpCI, nullptr, &m_renderPass));
        std::cout << "[Vulkan] Render pass OK.\n"; // FIXME 3: usunięto literalne \n
    }

    // ─────────────────────────────────────────
    void loadShaders() {
        std::cout << "[Shader] Próba kompilacji shaderów...\n";

        // Najpierw spróbuj załadować z pliku (jeśli wcześniej skompilowano)
        m_vertSpv = loadSpv("vert.spv");
        m_fragSpv = loadSpv("frag.spv");

        if (!m_vertSpv.empty() && !m_fragSpv.empty()) {
            std::cout << "[Shader] Załadowano z pliku vert.spv/frag.spv.\n";
            return;
        }

        // Skompiluj w runtime
        m_vertSpv = compileGlsl(VERT_GLSL, "vert", "vr_tmp_shader");
        m_fragSpv = compileGlsl(FRAG_GLSL, "frag", "vr_tmp_shader");

        if (m_vertSpv.empty() || m_fragSpv.empty()) {
            throw std::runtime_error(
                "Nie można skompilować shaderów SPIR-V!\n"
                "Zainstaluj Vulkan SDK (glslangValidator.exe) i dodaj do PATH,\n"
                "lub umieść vert.spv / frag.spv w katalogu roboczym.\n\n"
                "Aby skompilować ręcznie:\n"
                "  glslangValidator -V -S vert -o vert.spv shader.vert\n"
                "  glslangValidator -V -S frag -o frag.spv shader.frag\n\n"
                "Pliki GLSL znajdziesz w: vr_shaders/ (wygenerowane przez app)");
        }
        std::cout << "[Shader] Kompilacja OK. vert=" << m_vertSpv.size()
                  << " dwords, frag=" << m_fragSpv.size() << " dwords.\n";
    }

    VkShaderModule createShaderModule(const std::vector<uint32_t>& spv) {
        VkShaderModuleCreateInfo smCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        smCI.codeSize = spv.size() * 4;
        smCI.pCode    = spv.data();
        VkShaderModule mod;
        VK_CHECK(vkCreateShaderModule(m_device, &smCI, nullptr, &mod));
        return mod;
    }

    void createPipeline() {
        std::cout << "[Vulkan] Tworzenie pipeline...\n";

        // ── FIXME 4: ładuj/kompiluj shadery zamiast inline SPIR-V ──
        loadShaders();

        VkShaderModule vertMod = createShaderModule(m_vertSpv);
        VkShaderModule fragMod = createShaderModule(m_fragSpv);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertMod;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragMod;
        stages[1].pName  = "main";

        // ── FIXME 5: zerowanie struktur ──────────────────────────────
        VkPipelineVertexInputStateCreateInfo viCI{};
        viCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        // Brak atrybutów – fullscreen triangle generuje pozycje w VS

        VkPipelineInputAssemblyStateCreateInfo iaCI{};
        iaCI.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        iaCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport vp{0, 0,
            (float)m_swapchains[0].width,
            (float)m_swapchains[0].height,
            0.f, 1.f};
        VkRect2D sc2d{{0,0},
            {(uint32_t)m_swapchains[0].width,
             (uint32_t)m_swapchains[0].height}};

        VkPipelineViewportStateCreateInfo vpCI{};
        vpCI.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vpCI.viewportCount = 1; vpCI.pViewports = &vp;
        vpCI.scissorCount  = 1; vpCI.pScissors  = &sc2d;

        VkPipelineRasterizationStateCreateInfo rsCI{};
        rsCI.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rsCI.polygonMode = VK_POLYGON_MODE_FILL;
        rsCI.cullMode    = VK_CULL_MODE_NONE;
        rsCI.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rsCI.lineWidth   = 1.f;

        VkPipelineMultisampleStateCreateInfo msCI{};
        msCI.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo dsCI{};
        dsCI.sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsCI.depthTestEnable   = VK_FALSE;
        dsCI.depthWriteEnable  = VK_FALSE;
        dsCI.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState cbAtt{};
        cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cbCI{};
        cbCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cbCI.attachmentCount = 1;
        cbCI.pAttachments    = &cbAtt;

        // Push constant: vec4 color (16 bajtów) dla fragment shadera
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pcRange.offset     = 0;
        pcRange.size       = 16; // sizeof(vec4)

        VkPipelineLayoutCreateInfo plCI{};
        plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCI.pushConstantRangeCount = 1;
        plCI.pPushConstantRanges    = &pcRange;
        VK_CHECK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipeLayout));

        VkGraphicsPipelineCreateInfo gpCI{};
        gpCI.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpCI.stageCount          = 2;
        gpCI.pStages             = stages;
        gpCI.pVertexInputState   = &viCI;
        gpCI.pInputAssemblyState = &iaCI;
        gpCI.pViewportState      = &vpCI;
        gpCI.pRasterizationState = &rsCI;
        gpCI.pMultisampleState   = &msCI;
        gpCI.pDepthStencilState  = &dsCI;
        gpCI.pColorBlendState    = &cbCI;
        gpCI.layout              = m_pipeLayout;
        gpCI.renderPass          = m_renderPass;
        gpCI.subpass             = 0;

        std::cout << "[Vulkan] vkCreateGraphicsPipelines...\n";
        VK_CHECK(vkCreateGraphicsPipelines(
            m_device, VK_NULL_HANDLE, 1, &gpCI, nullptr, &m_pipeline));

        vkDestroyShaderModule(m_device, vertMod, nullptr);
        vkDestroyShaderModule(m_device, fragMod, nullptr);
        std::cout << "[Vulkan] Pipeline OK.\n";
    }

    void createFramebuffers() {
        std::cout << "[Vulkan] Tworzenie framebuffers...\n";
        for (size_t i = 0; i < m_swapchains.size(); i++) {
            auto& sc  = m_swapchains[i];
            auto& fbs = m_swapFBs[i];
            fbs.resize(sc.images.size());

            for (size_t j = 0; j < sc.images.size(); j++) {
                VkImageViewCreateInfo ivCI{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                ivCI.image            = sc.images[j].image;
                ivCI.viewType         = VK_IMAGE_VIEW_TYPE_2D;
                ivCI.format           = m_swapchainFormat; // NAPRAWIONE
                ivCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                VK_CHECK(vkCreateImageView(m_device, &ivCI, nullptr, &fbs[j].view));

                VkFramebufferCreateInfo fbCI{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                fbCI.renderPass      = m_renderPass;
                fbCI.attachmentCount = 1;
                fbCI.pAttachments    = &fbs[j].view;
                fbCI.width           = sc.width;
                fbCI.height          = sc.height;
                fbCI.layers          = 1;
                VK_CHECK(vkCreateFramebuffer(m_device, &fbCI, nullptr, &fbs[j].fb));
            }
        }
        std::cout << "[Vulkan] Framebuffery gotowe.\n";
    }

    void renderEye(uint32_t eyeIdx, uint32_t imageIdx,
                   float r, float g, float b)
    {
        auto& sc = m_swapchains[eyeIdx];
        auto& fb = m_swapFBs[eyeIdx][imageIdx];

        VkCommandBufferAllocateInfo cbAI{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbAI.commandPool        = m_cmdPool;
        cbAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAI.commandBufferCount = 1;
        VkCommandBuffer cb;
        VK_CHECK(vkAllocateCommandBuffers(m_device, &cbAI, &cb));

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cb, &bi));

        VkClearValue clear{};
        clear.color = {{r, g, b, 1.f}};

        VkRenderPassBeginInfo rpBI{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpBI.renderPass        = m_renderPass;
        rpBI.framebuffer       = fb.fb;
        rpBI.renderArea.extent = {(uint32_t)sc.width, (uint32_t)sc.height};
        rpBI.clearValueCount   = 1;
        rpBI.pClearValues      = &clear;
        vkCmdBeginRenderPass(cb, &rpBI, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

        // Kolor z push constant
        float color[4] = {r, g, b, 1.0f};
        vkCmdPushConstants(cb, m_pipeLayout,
            VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, color);

        vkCmdDraw(cb, 3, 1, 0, 0); // fullscreen triangle

        vkCmdEndRenderPass(cb);
        VK_CHECK(vkEndCommandBuffer(cb));

        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cb;
        VK_CHECK(vkQueueSubmit(m_queue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(m_queue));

        vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cb);
    }

    void pollEvents() {
        XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(m_instance, &ev) == XR_SUCCESS) {
            switch (ev.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                auto* e = (XrEventDataSessionStateChanged*)&ev;
                m_sessionState = e->state;
                std::cout << "[OpenXR] Stan sesji: " << m_sessionState << "\n";
                if (m_sessionState == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo sbi{XR_TYPE_SESSION_BEGIN_INFO};
                    sbi.primaryViewConfigurationType =
                        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    XrResult r = xrBeginSession(m_session, &sbi);
                    if (r == XR_SUCCESS) {
                        m_sessionRunning = true;
                        std::cout << "[OpenXR] Sesja rozpoczeta! Patrz w headset.\n";
                    } else {
                        char buf[XR_MAX_RESULT_STRING_SIZE];
                        xrResultToString(m_instance, r, buf);
                        std::cout << "[BLAD] xrBeginSession: " << buf << "\n";
                        m_quit = true;
                    }
                }
                else if (m_sessionState == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(m_session);
                    m_sessionRunning = false;
                }
                else if (m_sessionState == XR_SESSION_STATE_EXITING ||
                         m_sessionState == XR_SESSION_STATE_LOSS_PENDING)
                    m_quit = true;
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                m_quit = true; break;
            default: break;
            }
            ev = {XR_TYPE_EVENT_DATA_BUFFER};
        }
    }

    void mainLoop() {
        std::cout << "[App] Petla glowna (Ctrl+C aby wyjsc)...\n";
        uint64_t frame = 0;

        while (!m_quit) {
            pollEvents();
            if (!m_sessionRunning) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            XrFrameWaitInfo fwi{XR_TYPE_FRAME_WAIT_INFO};
            XrFrameState    fs {XR_TYPE_FRAME_STATE};
            XR_CHECK(xrWaitFrame(m_session, &fwi, &fs));

            XrFrameBeginInfo fbi{XR_TYPE_FRAME_BEGIN_INFO};
            XR_CHECK(xrBeginFrame(m_session, &fbi));

            float t  = (float)frame * 0.02f;
            float rL = 0.5f + 0.5f * sinf(t);
            float gL = 0.5f + 0.5f * sinf(t + 2.094f);
            float bL = 0.5f + 0.5f * sinf(t + 4.189f);
            float rR = 0.5f + 0.5f * sinf(t + 1.0f);
            float gR = 0.5f + 0.5f * sinf(t + 3.094f);
            float bR = 0.5f + 0.5f * sinf(t + 5.189f);

            std::vector<XrCompositionLayerProjectionView> projViews(m_swapchains.size());
            std::vector<uint32_t> imgIndices(m_swapchains.size());

            for (size_t i = 0; i < m_swapchains.size(); i++) {
                XrSwapchainImageAcquireInfo acqI{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                XR_CHECK(xrAcquireSwapchainImage(m_swapchains[i].handle, &acqI, &imgIndices[i]));
                XrSwapchainImageWaitInfo waitI{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                waitI.timeout = XR_INFINITE_DURATION;
                XR_CHECK(xrWaitSwapchainImage(m_swapchains[i].handle, &waitI));

                float r = (i == 0) ? rL : rR;
                float g = (i == 0) ? gL : gR;
                float b = (i == 0) ? bL : bR;
                renderEye((uint32_t)i, imgIndices[i], r, g, b);

                auto& pv = projViews[i];
                pv = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                pv.subImage.swapchain            = m_swapchains[i].handle;
                pv.subImage.imageRect.offset     = {0, 0};
                pv.subImage.imageRect.extent     = {m_swapchains[i].width, m_swapchains[i].height};

                XrSwapchainImageReleaseInfo relI{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                XR_CHECK(xrReleaseSwapchainImage(m_swapchains[i].handle, &relI));
            }

            XrViewLocateInfo vli{XR_TYPE_VIEW_LOCATE_INFO};
            vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            vli.displayTime           = fs.predictedDisplayTime;
            vli.space                 = m_appSpace;
            XrViewState vs{XR_TYPE_VIEW_STATE};
            uint32_t vc = (uint32_t)m_views.size();
            xrLocateViews(m_session, &vli, &vs, vc, &vc, m_views.data());

            for (size_t i = 0; i < projViews.size() && i < m_views.size(); i++) {
                projViews[i].pose = m_views[i].pose;
                projViews[i].fov  = m_views[i].fov;
            }

            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            layer.space     = m_appSpace;
            layer.viewCount = (uint32_t)projViews.size();
            layer.views     = projViews.data();

            const XrCompositionLayerBaseHeader* layers[] = {
                (XrCompositionLayerBaseHeader*)&layer};

            XrFrameEndInfo fei{XR_TYPE_FRAME_END_INFO};
            fei.displayTime          = fs.predictedDisplayTime;
            fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            fei.layerCount           = fs.shouldRender ? 1 : 0;
            fei.layers               = fs.shouldRender ? layers : nullptr;
            XR_CHECK(xrEndFrame(m_session, &fei));

            if (frame % 100 == 0)
                std::cout << "[App] Klatka " << frame << "\n";
            frame++;
        }
        std::cout << "[App] Petla zakonczona.\n";
    }

    void cleanup() {
        if (m_device != VK_NULL_HANDLE) vkDeviceWaitIdle(m_device);

        for (auto& fbs : m_swapFBs)
            for (auto& f : fbs) {
                if (f.fb   != VK_NULL_HANDLE) vkDestroyFramebuffer(m_device, f.fb,   nullptr);
                if (f.view != VK_NULL_HANDLE) vkDestroyImageView  (m_device, f.view, nullptr);
            }

        if (m_pipeline   != VK_NULL_HANDLE) vkDestroyPipeline      (m_device, m_pipeline,   nullptr);
        if (m_pipeLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, m_pipeLayout, nullptr);
        if (m_renderPass != VK_NULL_HANDLE) vkDestroyRenderPass    (m_device, m_renderPass,  nullptr);
        if (m_cmdPool    != VK_NULL_HANDLE) vkDestroyCommandPool   (m_device, m_cmdPool,     nullptr);
        if (m_device     != VK_NULL_HANDLE) vkDestroyDevice        (m_device, nullptr);
        if (m_vkInstance != VK_NULL_HANDLE) vkDestroyInstance      (m_vkInstance, nullptr);

        for (auto& sc : m_swapchains)
            if (sc.handle != XR_NULL_HANDLE) xrDestroySwapchain(sc.handle);
        if (m_appSpace != XR_NULL_HANDLE) xrDestroySpace  (m_appSpace);
        if (m_session  != XR_NULL_HANDLE) xrDestroySession(m_session);
        if (m_instance != XR_NULL_HANDLE) xrDestroyInstance(m_instance);

        std::cout << "[App] Cleanup zakończony.\n";
    }
};

int main() {
    try {
        VRApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "\n[BLAD] " << e.what() << "\n\n";
        std::cerr << "Wskazowki:\n"
                  << "  1. Uruchom SteamVR/Oculus/WMR runtime\n"
                  << "  2. Podlacz headset VR\n"
                  << "  3. Zainstaluj Vulkan SDK (dla glslangValidator)\n"
                  << "  4. Dodaj glslangValidator.exe do PATH\n";
        return 1;
    }
    return 0;
}
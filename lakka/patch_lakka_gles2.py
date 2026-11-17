#!/usr/bin/env python3
"""GLES2/Mali-400 indexed texture renderer for pinned FITD-R36S release."""
from pathlib import Path
import sys

root=Path(sys.argv[1]).resolve() if len(sys.argv)==2 else None
if not root or not (root/'FitdLib/rendererBGFX.cpp').is_file():
    raise SystemExit('usage: patch_lakka_gles2.py PATCHED_FITD_SOURCE')
osys=root/'FitdLib/osystemSDL.cpp'
s=osys.read_text()
old='SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);'
assert s.count(old)==1, 'SDL3 EGL context anchor changed'
osys.write_text(s.replace(old,'SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);',1))
p=root/'FitdLib/rendererBGFX.cpp'
s=p.read_text()
anchor='void osystem_setPalette(u8* palette)'
assert s.count(anchor)==1
helper=r'''#include <cstdint>
#include <vector>
// RGBA-format normalized greyscale index upload for GLES2.
static const bgfx::Memory* lakkaPackIndexes(const uint8_t* indexed,size_t n) {
    std::vector<uint8_t> pixels(n*4);
    for(size_t i=0;i<n;++i) {
        uint8_t v=indexed[i];
        pixels[i*4]=v; pixels[i*4+1]=v; pixels[i*4+2]=v; pixels[i*4+3]=255;
    }
    return bgfx::copy(pixels.data(),uint32_t(pixels.size()));
}
'''
s=s.replace(anchor,helper+anchor,1)
assert s.count('bgfx::TextureFormat::R8U')==5, 'Expected 5 indexed GPU textures'
s=s.replace('bgfx::TextureFormat::R8U','bgfx::TextureFormat::RGBA8')
uploads={
'bgfx::copy(RGB_Pal, 256 * 3)': ('lakkaPackIndexes((const uint8_t*)RGB_Pal,256 * 3)',2),
'bgfx::copy(uiLayer.data(), 320 * 200)': ('lakkaPackIndexes(uiLayer.data(),320 * 200)',1),
'bgfx::copy(physicalScreen, 320 * 200)': ('lakkaPackIndexes(physicalScreen,320 * 200)',1),
'bgfx::copy(noiseTextureData.data(), noiseTextureDim * noiseTextureDim)': ('lakkaPackIndexes(noiseTextureData.data(),noiseTextureDim * noiseTextureDim)',1),
'bgfx::copy(mask.data(), 320 * 200)': ('lakkaPackIndexes(mask.data(),320 * 200)',1)}
for src,(dst,n) in uploads.items():
    assert s.count(src)==n, f'Upload anchor mismatch: {src} = {s.count(src)}, expected {n}'
    s=s.replace(src,dst)
p.write_text(s)
sh=root/'FitdLib/shaders'
f={
'palette.sh':'''SAMPLER2D(s_paletteTexture,1);
vec4 getColorFromRawOffset(float idx) {
    float y=(floor(idx+0.5)+0.5)/256.0;
    return vec4(texture2D(s_paletteTexture,vec2(0.5/3.0,y)).r,
                texture2D(s_paletteTexture,vec2(1.5/3.0,y)).r,
                texture2D(s_paletteTexture,vec2(2.5/3.0,y)).r,1.0);
}
vec4 getColor(int bank,int color) { return getColorFromRawOffset(float(bank*16+color)); }
''',
'background_ps.sc':'''$input v_texcoord0
#include "bgfx_shader.sh"
#include "palette.sh"
SAMPLER2D(s_backgroundTexture,0);
void main() {
    float idx=floor(texture2D(s_backgroundTexture,v_texcoord0).r*255.0+0.5);
    gl_FragColor=getColorFromRawOffset(idx);
}
''',
'ui_ps.sc':'''$input v_texcoord0
#include "bgfx_shader.sh"
#include "palette.sh"
SAMPLER2D(s_backgroundTexture,0);
void main() {
    float idx=floor(texture2D(s_backgroundTexture,v_texcoord0).r*255.0+0.5);
    if(idx<0.5) discard;
    gl_FragColor=getColorFromRawOffset(idx);
}
''',
'maskBackground_ps.sc':'''$input v_texcoord0
#include "bgfx_shader.sh"
#include "palette.sh"
SAMPLER2D(s_maskPaletteTexture,0);
SAMPLER2D(s_backgroundTexture,2);
void main() {
    float m=floor(texture2D(s_maskPaletteTexture,v_texcoord0).r*255.0+0.5);
    if(m<0.5) discard;
    float idx=floor(texture2D(s_backgroundTexture,v_texcoord0).r*255.0+0.5);
    gl_FragColor=getColorFromRawOffset(idx);
}
''',
'noise_ps.sc':'''$input v_texcoord0, v_screenSpacePosition
#include "bgfx_shader.sh"
#include "palette.sh"
SAMPLER2D(s_noiseTexture,0);
void main() {
    int c=int(v_texcoord0.x*15.0);
    int b=int(v_texcoord0.y*15.0);
    vec2 n=mod(floor(v_screenSpacePosition.xy*v_screenSpacePosition.z),255.0);
    float noise=texture2D(s_noiseTexture,(n+vec2(0.5))/256.0).r;
    gl_FragColor=getColor(b,c)*(0.5+noise*0.5);
}
''',
'sphere_ps.sc':'''$input v_texcoord0, v_sphereParams, v_screenSpacePosition
#include "bgfx_shader.sh"
#include "palette.sh"
void main() {
    vec2 pos=(v_screenSpacePosition.xy-v_sphereParams.xy)/max(v_sphereParams.z,0.001);
    pos.x*=5.0/6.0;
    if(dot(pos,pos)>1.0) discard;
    int c=int(v_texcoord0.x*15.0);
    int b=int(v_texcoord0.y*15.0);
    float m=v_sphereParams.w;
    if(abs(m-3.0)<0.5) c=int(((pos.x/sqrt(max(1.0-pos.y*pos.y,0.0001)))*0.5+0.5)*15.0);
    gl_FragColor=getColor(b,c);
    if(abs(m-2.0)<0.5) gl_FragColor.a=0.5;
}
'''
}
for name,content in f.items():
    assert (sh/name).exists(),f'Missing shader {name}'
    (sh/name).write_text(content)
for p in sh.glob('*_ps.sc'):
    if any(x in p.read_text() for x in ('USAMPLER2D','texelFetch','uvec','uint(')):
        raise SystemExit('GLES3 shader token remains: '+str(p))
print('GLES2 texture/shader rewrite complete; device tests pending')

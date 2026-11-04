// Bounded parser for original FITD sBody polygon models.
//
// File structure follows upstream FitdLib/hqr.cpp::createBodyFromPtr()
// in yaz0r/FITD commit 543e22bc78b64405537ae545ecd382dcdf2c12b2.
// This is for the experimental Android OpenGL ES 2.0 model-preview renderer;
// it does not replace the original engine animation/scene processing.
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <utility>

namespace fitd_body {
struct Vertex {float x, y, z;};
struct Primitive {
    std::vector<uint16_t> vertices;
    uint8_t color=0, material=0;
};
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Primitive> primitives;
    float minX=0, minY=0, minZ=0, maxX=0, maxY=0, maxZ=0;
};
class Reader {
    const uint8_t* data; size_t size,offset=0;
public:
    Reader(const uint8_t* p,size_t n):data(p),size(n){}
    bool skip(size_t n){
        if(!data||offset>size||n>size-offset)return false;
        offset+=n;return true;
    }
    bool get8(uint8_t& value){
        if(!data||offset>=size)return false;
        value=data[offset++];return true;
    }
    bool get16(uint16_t& value){
        if(!data||offset>size||size-offset<2)return false;
        value=(uint16_t)data[offset]|
              ((uint16_t)data[offset+1]<<8);
        offset+=2;return true;
    }
    bool getS16(int16_t& value){
        uint16_t raw;
        if(!get16(raw))return false;
        value=(int16_t)raw;return true;
    }
};
inline bool parse(const uint8_t* bytes,size_t size,Mesh& target){
    target=Mesh{};
    if(!bytes||size<22||size>524288)return false;
    Reader p(bytes,size);
    uint16_t flags=0,scratch=0,numVertices=0,numGroups=0,numPrimitives=0;
    if(!p.get16(flags)||!p.skip(12)||!p.get16(scratch))return false;
    if(scratch>8192||!p.skip(scratch)||!p.get16(numVertices)||
       numVertices<3||numVertices>1200)return false;
    Mesh model;
    model.vertices.reserve(numVertices);
    for(unsigned i=0;i<numVertices;++i){
        int16_t x,y,z;
        if(!p.getS16(x)||!p.getS16(y)||!p.getS16(z))return false;
        model.vertices.push_back(Vertex{(float)x,(float)y,(float)z});
    }
    if(flags&4)return false; // INFO_TORTUE is unsupported upstream
    if(flags&2){ // INFO_ANIM
        if(!p.get16(numGroups)||numGroups>60)return false;
        const size_t sizePerGroup=(flags&8)?24:16; // INFO_OPTIMISE
        if(!p.skip((size_t)numGroups*2+numGroups*sizePerGroup))return false;
    }
    if(!p.get16(numPrimitives)||numPrimitives<1||numPrimitives>1400)return false;
    for(unsigned i=0;i<numPrimitives;++i){
        uint8_t type=0,count=0,material=0,color=0;
        if(!p.get8(type))return false;
        switch(type){
            case 0: // line
                if(!p.get8(material)||!p.get8(color)||!p.skip(1))return false;
                count=2;break;
            case 1: case 8: case 9: case 10: // polygons
                if(!p.get8(count)||!p.get8(material)||!p.get8(color))return false;
                break;
            case 2: case 6: case 7: // point, bigpoint, zixel
                if(!p.get8(material)||!p.get8(color)||!p.skip(1))return false;
                count=1;break;
            case 3: // sphere
                if(!p.get8(material)||!p.get8(color)||!p.skip(3))return false;
                count=1;break;
            default: return false;
        }
        if(count==0||count>80)return false;
        Primitive element;
        element.material=material;element.color=color;
        element.vertices.reserve(count);
        for(unsigned n=0;n<count;++n){
            uint16_t offset;
            if(!p.get16(offset)||offset%6!=0||offset/6>=numVertices)return false;
            element.vertices.push_back((uint16_t)(offset/6));
        }
        if((type==9||type==10)&&!p.skip((size_t)count*2))return false;
        if(type==1||type==8||type==9||type==10||type==0)
            model.primitives.push_back(std::move(element));
    }
    if(model.primitives.empty())return false;
    model.minX=model.maxX=model.vertices[0].x;
    model.minY=model.maxY=model.vertices[0].y;
    model.minZ=model.maxZ=model.vertices[0].z;
    for(const Vertex& v:model.vertices){
        model.minX=std::min(model.minX,v.x);
        model.minY=std::min(model.minY,v.y);
        model.minZ=std::min(model.minZ,v.z);
        model.maxX=std::max(model.maxX,v.x);
        model.maxY=std::max(model.maxY,v.y);
        model.maxZ=std::max(model.maxZ,v.z);
    }
    if(model.maxY-model.minY<10.f||model.maxX-model.minX<5.f)return false;
    target=std::move(model);
    return true;
}
} // namespace fitd_body

#pragma once
#include "platform/image.hpp"
namespace fr {
// Local Collada bind-pose preview. Original geometry and UVs remain unchanged.
class BallModel {
public:
    void load(const std::filesystem::path& dae);
    bool empty()const{return vertices_.empty();}
    size_t triangles()const{return vertices_.size()/3;}
    Image render(float yaw,float pitch,int width=256,int height=200)const;
private:
    struct Vertex {float x,y,z,u,v,nx,ny,nz;};
    std::vector<Vertex> vertices_;Image texture_;
};
}

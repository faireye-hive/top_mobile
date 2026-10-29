#include "top/GfxDevice.h"
#include "top/GlxWindow.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

std::array<top::MeshVertex, 6> quad(float left, float right, float bottom, float top, int color) {
    std::array<top::MeshVertex, 6> vertices{};
    const float xy[6][2] = {{left,bottom},{right,bottom},{left,top},{left,top},{right,bottom},{right,top}};
    for (int i=0;i<6;++i) {
        auto& v=vertices[i]; v.x=xy[i][0];v.y=xy[i][1];v.a=1.f;
        v.r=color==0;v.g=color==1;v.b=color==2;
    }
    return vertices;
}
int main() {
    top::GlxWindow window;
    if (!window.open(64,64,"Render cache regression")) return 77;
    top::GfxDevice device;
    assert(device.init());device.resize(64,64);
    const auto red=quad(-1,0,-1,1,0),blue=quad(0,1,-1,1,2),green=quad(-.15f,.15f,-.15f,.15f,1);
    const unsigned left=device.uploadStaticMesh(0,red.data(),6),right=device.uploadActorMesh(0,blue.data(),6);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float shifted[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,.5f,0,1};
    for (int frame=0;frame<3;++frame) {
        device.resetFrameStats();device.beginFrame(0,0,0);
        device.drawStaticMesh(left,6,identity,0,0,0);
        // Uploading another VBO must not modify the first VAO's attribute sources.
        device.uploadActorMesh(right,blue.data(),6);
        device.drawStaticMesh(right,6,identity,0,0,0);
        device.draw(green.data(),6,shifted,0,0,0);
        std::vector<std::uint8_t> rgb(64*64*3);
        assert(device.readPixels(rgb.data(),64,64));
        auto pixel=[&](int x,int y,int c){return rgb[(y*64+x)*3+c];};
        assert(pixel(8,32,0)>240 && pixel(8,32,1)<5);
        assert(pixel(56,32,2)>240 && pixel(56,32,0)<5);
        assert(pixel(32,16,1)>240 && pixel(32,16,0)<5);
        const auto stats=device.frameStats();
        assert(stats.draws==3 && stats.uniformUpdates==8);
        assert(stats.attributeSetups==(frame==0?2u:0u));
    }
    device.deleteStaticMesh(left);device.deleteStaticMesh(right);
    device.shutdown();window.close();
    std::puts("VAO retention, matrix/uniform changes and static/dynamic interleaving: OK");
}

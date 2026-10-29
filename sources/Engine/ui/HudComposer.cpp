#include "HudComposer.h"
#include "screens/InventoryLayout.h"
#include <algorithm>
#include <cmath>

namespace top::ui {
namespace {
void vertex(std::vector<MeshVertex>& verts, float x, float y, float w, float h,
            float r, float g, float b, float u, float v) {
    MeshVertex p{};
    p.x = x * 2.f / w - 1.f;
    p.y = 1.f - y * 2.f / h;
    p.r = r; p.g = g; p.b = b; p.a = 1.f;
    p.u = u; p.v = v;
    verts.push_back(p);
}
}
HudComposer::HudComposer(std::vector<ActorBatch>& out, const Font& font,
                         const std::string& root, int w, int h)
    : batches_(out), font_(font), root_(root), width_(float(w)), height_(float(h)) {}
ActorBatch& HudComposer::batch(const std::string& texture) {
    if (!batches_.empty() && batches_.back().texture == texture && batches_.back().mode == 5)
        return batches_.back();
    ActorBatch out;
    out.texture = texture;
    out.mode = 5;
    batches_.push_back(std::move(out));
    return batches_.back();
}
void HudComposer::texturedRect(const std::string& texture,
                               float x, float y, float w, float h,
                               float u0, float v0, float u1, float v1,
                               HudClip clip) {
    if (w <= 0.f || h <= 0.f || clip.w <= 0.f || clip.h <= 0.f) return;
    const float left = std::max({x, clip.x, 0.f});
    const float right = std::min({x + w, clip.x + clip.w, width_});
    const float top = std::max({y, clip.y, 0.f});
    const float bottom = std::min({y + h, clip.y + clip.h, height_});
    if (right <= left || bottom <= top) return;
    const float a = u0 + (u1-u0)*(left-x)/w;
    const float b = u0 + (u1-u0)*(right-x)/w;
    const float c = v0 + (v1-v0)*(top-y)/h;
    const float d = v0 + (v1-v0)*(bottom-y)/h;
    auto& verts = batch(texture).verts;
    vertex(verts,left,top,width_,height_,1.f,1.f,1.f,a,c);
    vertex(verts,right,top,width_,height_,1.f,1.f,1.f,b,c);
    vertex(verts,left,bottom,width_,height_,1.f,1.f,1.f,a,d);
    vertex(verts,right,top,width_,height_,1.f,1.f,1.f,b,c);
    vertex(verts,right,bottom,width_,height_,1.f,1.f,1.f,b,d);
    vertex(verts,left,bottom,width_,height_,1.f,1.f,1.f,a,d);
}
void HudComposer::coloredRect(float x, float y, float w, float h,
                              float r, float g, float b, HudClip clip) {
    if (w <= 0.f || h <= 0.f) return;
    const float left = std::max({x, clip.x, 0.f});
    const float right = std::min({x+w,clip.x+clip.w,width_});
    const float top = std::max({y,clip.y,0.f});
    const float bottom = std::min({y+h,clip.y+clip.h,height_});
    if (left >= right || top >= bottom) return;
    auto& v = batch("white").verts;
    vertex(v,left,top,width_,height_,r,g,b,0.f,0.f);
    vertex(v,right,top,width_,height_,r,g,b,0.f,0.f);
    vertex(v,left,bottom,width_,height_,r,g,b,0.f,0.f);
    vertex(v,right,top,width_,height_,r,g,b,0.f,0.f);
    vertex(v,right,bottom,width_,height_,r,g,b,0.f,0.f);
    vertex(v,left,bottom,width_,height_,r,g,b,0.f,0.f);
}
void HudComposer::glyphs(const std::string& value, float x, float y,
                         float r, float g, float b, HudClip clip) {
    if (!font_.ready()) return;
    float pen = x;
    for (unsigned char ch : value) {
        Font::Glyph glyph;
        if (!font_.glyph(static_cast<char>(ch), glyph)) continue;
        const float gx = pen + glyph.left;
        const float gy = y + font_.ascent() - glyph.top;
        if (!glyph.empty && glyph.w > 0 && glyph.h > 0) {
            const float x0 = std::max({gx,clip.x,0.f});
            const float y0 = std::max({gy,clip.y,0.f});
            const float x1 = std::min({gx+glyph.w,clip.x+clip.w,width_});
            const float y1 = std::min({gy+glyph.h,clip.y+clip.h,height_});
            if (x1 > x0 && y1 > y0) {
                float u0 = glyph.u0+(glyph.u1-glyph.u0)*(x0-gx)/glyph.w;
                float u1 = glyph.u0+(glyph.u1-glyph.u0)*(x1-gx)/glyph.w;
                float v0 = glyph.v0+(glyph.v1-glyph.v0)*(y0-gy)/glyph.h;
                float v1 = glyph.v0+(glyph.v1-glyph.v0)*(y1-gy)/glyph.h;
                auto& verts=batch("font").verts;
                auto quad=[&](float dx,float dy,float cr,float cg,float cb) {
                    vertex(verts,x0+dx,y0+dy,width_,height_,cr,cg,cb,u0,v0);
                    vertex(verts,x1+dx,y0+dy,width_,height_,cr,cg,cb,u1,v0);
                    vertex(verts,x0+dx,y1+dy,width_,height_,cr,cg,cb,u0,v1);
                    vertex(verts,x1+dx,y0+dy,width_,height_,cr,cg,cb,u1,v0);
                    vertex(verts,x1+dx,y1+dy,width_,height_,cr,cg,cb,u1,v1);
                    vertex(verts,x0+dx,y1+dy,width_,height_,cr,cg,cb,u0,v1);
                };
                // Text shadow uses the same clipping as the glyph so it cannot
                // leave a hidden or obscured window behind.
                quad(0.5f,0.5f,0.f,0.f,0.f);
                quad(0.f,0.f,r,g,b);
            }
        }
        pen += glyph.advance;
        if (pen > clip.x+clip.w+font_.line()) break;
    }
}
void HudComposer::layer(UIRenderer& shapes, const HudDrawList& draw, HudClip clip) {
    for (const HudSprite& s : draw.sprites) {
        if (!s.file || s.texW <= 0 || s.texH <= 0) continue;
        std::string path = std::string(s.file) == "minimap" ? "minimap" : root_ + "/" + s.file;
        texturedRect(path,s.x,s.y,s.w,s.h,
                     s.su/float(s.texW),s.sv/float(s.texH),
                     (s.su+s.sw)/float(s.texW),(s.sv+s.sh)/float(s.texH),clip);
    }
    if (!shapes.empty()) batches_.push_back(shapes.takeBatch());
    for (const HudIcon& icon : draw.icons) {
        if (icon.file.empty()) continue;
        texturedRect(root_ + "/" + icon.file,icon.x,icon.y,icon.w,icon.h,
                     0.f,0.f,1.f,1.f,clip);
    }
    for (const HudLabel& label : draw.labels) {
        if (label.kind == HudLabel::Kind::CountBadge) {
            if (label.count <= 1) continue;
            const std::string n=std::to_string(label.count);
            float tw=0.f;
            for (unsigned char ch : n) { Font::Glyph gl; if (font_.glyph(char(ch),gl)) tw+=gl.advance; }
            const float tx=label.x+inventory_layout::kIcon-tw-1.f;
            const float ty=label.y+inventory_layout::kIcon-std::max(10.f,font_.line()-2.f);
            coloredRect(tx-1.f,ty,tw+2.f,std::max(10.f,font_.line()-2.f),0.68f,0.96f,0.97f,clip);
            glyphs(n,tx,ty,0.f,0.f,0.f,clip);
        } else {
            glyphs(label.value,label.x,label.y,label.r,label.g,label.b,clip);
        }
    }
}
void HudComposer::notes(const std::vector<HudNote>& notes) {
    const HudClip screen{0.f,0.f,width_,height_};
    for (const HudNote& n : notes) glyphs(n.text,n.x,n.y,n.r,n.g,n.b,screen);
}
void HudComposer::text(const std::string& value, float x, float y,
                       float r,float g,float b) {
    glyphs(value,x,y,r,g,b,{0.f,0.f,width_,height_});
}
} // namespace top::ui

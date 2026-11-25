/* Motion reconstruction from consecutive completed 3D lists. This uses
   original geometry, UVs and viewports; the sprite/HUD mixer is independent. */
namespace {
struct Primitive {
    std::array<Vec,3> v;
    Surface s;
    std::array<uint32_t,0x1a> regs;
    unsigned select;
};
static uint64_t hash_value(uint64_t old,unsigned value) {
    return (old^value)*1099511628211ull;
}
static bool textured(const Primitive &p) { return (p.s.cmd&24)!=0; }
static float area2(const Primitive &p) {
    auto &a=p.v[0],&b=p.v[1],&c=p.v[2];
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}
static float coverage(const Primitive& p) {
    return std::clamp(std::abs(area2(p))*.5f,0.0f,3000.0f);
}
static uint32_t source_uv(const Vec &v) {
    if(v.rw==0 || !std::isfinite(v.rw)) return 0;
    return uint32_t(uint16_t(std::lround(v.u/v.rw)))<<16 | uint16_t(std::lround(v.v/v.rw));
}
static unsigned clipped(const Vec&v,unsigned width) {
    return (v.x<=.125f?1:0)|(v.x>=width-1.125f?2:0)|(v.y<=.125f?4:0)|(v.y>=382.875f?8:0);
}
static uint64_t material(const Primitive &p,unsigned width) {
    const auto &s=p.s;
    uint64_t h=hash_value(1469598103934665603ull,s.cmd);
    // Keep the mirror and the main road in separate coordinate systems.
    for(unsigned i=3;i<=8;i++) h=hash_value(h,p.regs[i]&65535);
    if(textured(p)) {
        h=hash_value(h,s.page|(s.bank<<8)); h=hash_value(h,s.sx|(s.sy<<10));
        h=hash_value(h,s.tw|(s.th<<8));
        std::array<uint32_t,3> uv;
        for(unsigned i=0;i<3;i++) {
            unsigned edge=clipped(p.v[i],width);
            uv[i]=edge ? 0xfff00000u|edge : source_uv(p.v[i]);
        }
        std::sort(uv.begin(),uv.end());
        for(auto id:uv) h=hash_value(h,id);
    } else if(!(s.cmd&256)) h=hash_value(h,pack(s.flat));
    return h;
}
static float correspondence(const Primitive &a,const Primitive &b,int rot,unsigned width) {
    const float area_a=area2(a),area_b=area2(b);
    if(area_a*area_b<=0 || std::abs(area_a+area_b)<.25f) return 1e10f;
    float relative=std::abs(area_a-area_b)/(std::abs(area_a)+std::abs(area_b));
    if(relative>.58f) return 1e10f;
    float score=8.f*relative,largest=0;
    for(int j=0;j<3;j++){
        const Vec &v=a.v[(j+rot)%3],&w=b.v[j];
        if(textured(b) && source_uv(v)!=source_uv(w)) {
            unsigned edge=clipped(v,width);
            if(!edge || edge!=clipped(w,width) || !v.rw || !w.rw) return 1e10f;
            float du=v.u/v.rw-w.u/w.rw,dv=v.v/v.rw-w.v/w.rw;
            if(std::abs(du)>900 || std::abs(dv)>900) return 1e10f;
            score+=(du*du+dv*dv)/3200.f;
        }
        float dx=v.x-w.x,dy=v.y-w.y,d2=dx*dx+dy*dy;
        largest=std::max(largest,d2);
        float scale=std::max(std::abs(v.z)+std::abs(w.z),1e-12f);
        float dz=std::abs(v.z-w.z)/scale;
        if(d2>72.f*72.f || dz>.32f) return 1e10f;
        score+=d2/3.f+2048.f*dz*dz;
        // Vertex-coloured strips often reuse identical rendering commands.
        if(b.s.cmd&256) for(unsigned k=0;k<4;k++) {
            float c=v.rgba[k]-w.rgba[k];
            score+=c*c/256.f;
        }
    }
    return score+.1f*largest;
}
static Vec halfway(const Vec &a,const Vec &b) {
    Vec v=b;
    v.x=(a.x+b.x)*.5f;v.y=(a.y+b.y)*.5f;
    v.z=(a.z+b.z)*.5f;
    v.rw=v.z>0 && a.rw!=1 && b.rw!=1 ? 1.f/v.z : (a.rw+b.rw)*.5f;
    if(a.rw!=0 && b.rw!=0) {
        v.u=(a.u/a.rw+b.u/b.rw)*(.5f*v.rw);
        v.v=(a.v/a.rw+b.v/b.rw)*(.5f*v.rw);
    }
    v.diff=(a.diff+b.diff)*.5f;
    for(unsigned i=0;i<4;i++) v.rgba[i]=(a.rgba[i]+b.rgba[i])*.5f;
    return v;
}
struct EdgeAnchor { float z,dx,dy,weight; uint64_t viewport; };
static uint64_t vertex_key(const Vec&v) {
    return uint64_t(uint32_t(std::lround(v.x*16)))<<32 | uint32_t(std::lround(v.y*16));
}
static uint64_t view_id(const Primitive &p) {
    uint64_t key=1469598103934665603ull;
    for(unsigned i=3;i<=8;i++) key=hash_value(key,p.regs[i]&65535);
    return key;
}
static bool compatible_depth(float a,float b) {
    return std::fabs(a-b)<.012f*std::max(std::fabs(a)+std::fabs(b),1e-8f);
}
static void weld_motion(const std::vector<Primitive>&source,std::vector<Primitive>&mid,
                        const std::vector<uint8_t>&assigned) {
    // A clip-plane intersection often moves in UV space, changing its
    // triangle signature. Give coincident vertices the same displacement
    // across material seams, including matching/unmatched triangle edges.
    std::unordered_map<uint64_t,std::vector<EdgeAnchor>> anchors;
    for(unsigned i=0;i<source.size();i++) if(assigned[i]){
        auto &p=source[i];
        auto viewport=view_id(p);
        for(unsigned j=0;j<3;j++){
            const Vec &v=p.v[j],&q=mid[i].v[j];
            auto &list=anchors[vertex_key(v)];
            auto it=std::find_if(list.begin(),list.end(),[&](const EdgeAnchor&e){
                return e.viewport==viewport && compatible_depth(e.z,v.z);
            });
            if(it==list.end()) list.push_back({v.z,q.x-v.x,q.y-v.y,1,viewport});
            else {it->dx+=q.x-v.x;it->dy+=q.y-v.y;it->weight++;}
        }
    }
    for(unsigned i=0;i<source.size();i++) {
        auto &p=source[i];auto viewport=view_id(p);

        for(unsigned j=0;j<3;j++) {
            auto &v=p.v[j];auto &q=mid[i].v[j];
            auto at=anchors.find(vertex_key(v));if(at==anchors.end()) continue;
            for(auto &e:at->second) if(e.viewport==viewport && compatible_depth(e.z,v.z)) {
                float dx=e.dx/e.weight,dy=e.dy/e.weight;
                q.x=v.x+dx;q.y=v.y+dy;
                break;
            }
        }

    }
}
struct Pairing { float cost; unsigned current,previous; int rotation; };
static bool interpolate_geometry(const std::vector<Primitive>& before,const std::vector<Primitive>& after,
                                 std::vector<Primitive>& mesh,ZRMotionStats& stats,unsigned width) {
    stats={unsigned(before.size()),unsigned(after.size()),0,0};
    if(before.empty() || after.empty() || before.size()>=32768 || after.size()>=32768) return false;
    std::unordered_map<uint64_t,std::vector<unsigned>> by_material;
    for(unsigned i=0;i<before.size();i++) by_material[material(before[i],width)].push_back(i);
    std::vector<Pairing> pairs;
    for(unsigned i=0;i<after.size();i++) {
        auto it=by_material.find(material(after[i],width));if(it==by_material.end()) continue;
        // Retain the four closest plausible assignments before resolving
        // conflicts globally. This avoids crossing two similar nearby objects.
        std::array<Pairing,4> best;
        for(auto &p:best) p.cost=1e9f;
        for(auto j:it->second) {
            for(int rot=0;rot<3;rot++){
                float score=correspondence(before[j],after[i],rot,width);
                if(score>=best.back().cost) continue;
                Pairing pair{score,i,j,rot};
                for(auto &p:best) if(pair.cost<p.cost) std::swap(pair,p);
            }
        }
        for(auto &p:best) if(p.cost<1e9f) pairs.push_back(p);
    }
    std::stable_sort(pairs.begin(),pairs.end(),[](const Pairing&a,const Pairing&b){return a.cost<b.cost;});
    std::vector<uint8_t> was_used(before.size()),was_set(after.size());
    mesh=after;
    float visible_area=0,matched_area=0,area_before=0;
    for(const auto&p:before) area_before+=coverage(p);
    for(const auto&p:after) visible_area+=coverage(p);
    for(auto p:pairs) {
        if(was_used[p.previous] || was_set[p.current]) continue;
        was_used[p.previous]=was_set[p.current]=1;
        const Primitive &a=before[p.previous],&b=after[p.current];
        Primitive &mid=mesh[p.current];
        float advance=0;
        for(int j=0;j<3;j++) {
            const Vec &v=a.v[(j+p.rotation)%3],&w=b.v[j];
            mid.v[j]=halfway(v,w);
            advance=std::max(advance,std::abs(v.x-w.x)+std::abs(v.y-w.y));
        }
        stats.matched++;
        if(advance>=.125f) stats.moving++;
        matched_area+=coverage(b);
    }
    // A scene change or camera switch has no temporal correspondence.
    // The caller keeps the exact original frame in that case.
    weld_motion(after,mesh,was_set);
    return stats.moving>=3 && stats.matched>=std::min(before.size(),after.size())*.32f
        && matched_area>=.34f*std::max(area_before,visible_area);
}
} // namespace



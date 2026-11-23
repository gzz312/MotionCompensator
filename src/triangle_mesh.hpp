#pragma once
#include "motion_field.hpp"
#include "dots.hpp"
#include <span>

namespace mc {

enum class MeshLife { Empty, Entering, Active, Leaving, Retired };

struct MeshNode {
    Vec2 position{},velocity{};
    uint64_t id{};
    float opacity{},age{},crowdedFor{};
    MeshLife life=MeshLife::Empty;
    float fadeDelay{},fadeDuration{};
    uint8_t motionRegion{};
};

struct MeshTriangle {std::array<uint32_t,3> node;};
struct MeshEdge {uint32_t first,second;};

struct MeshStats {
    size_t live{},entering{},leaving{},triangles{},degenerate{};
    uint64_t inletBirths{},centerBirths{},interiorBirths{},centerExits{},edgeExits{},retired{},merged{},rewired{};
    double movementMs{},indexMs{},birthMs{},recycleMs{},topologyMs{};
};

inline constexpr float meshEdgeReach=2.05f;
struct TriangleLattice {
    int columns{},rows{};
    Vec2 origin{};
    float spacing{};
    float rowStep() const {return spacing*.8660254038f;}
    Vec2 point(int column,int row) const {
        return {origin.x+(column+(row%2)*.5f)*spacing,origin.y+row*rowStep()};
    }
};
inline TriangleLattice triangleLattice(int width,int height,float spacing) {
    TriangleLattice lattice{0,0,{},spacing};
    if(width<=0||height<=0||!std::isfinite(spacing)||spacing<4)return lattice;
    lattice.columns=int(std::clamp(std::ceil(width/spacing)+6,4.f,64.f));
    lattice.rows=int(std::clamp(std::ceil(height/lattice.rowStep())+6,4.f,64.f));
    lattice.origin={(width-(lattice.columns-1)*spacing)*.5f,(height-(lattice.rows-1)*lattice.rowStep())*.5f};
    return lattice;
}
inline float liveMeshSpacing(int width,int height,float requested) {
    constexpr float visibleNodeBudget=900;
    const float densityFloor=std::sqrt(float(width)*height/(visibleNodeBudget*.8660254038f));
    return std::ceil(std::max({requested,densityFloor,width/56.f,height/(56.f*.8660254038f)}));
}
inline Vec2 meshVelocity(FieldSample sample,int height) {
    if(!sample.valid||!std::isfinite(sample.confidence)||!std::isfinite(sample.velocity.x)||!std::isfinite(sample.velocity.y))return {};
    const float weight=std::clamp(sample.motionWeight>=0?sample.motionWeight:sample.confidence,0.f,1.f);
    const float limit=std::max(240.f,height*.45f);
    return {std::clamp(sample.velocity.x,-limit,limit)*weight,std::clamp(sample.velocity.y,-limit,limit)*weight};
}
inline FieldSample compensateMeshSample(FieldSample sample,float signedGain) {
    const float scale=-.35f*signedGain;
    sample.velocity={sample.velocity.x*scale,sample.velocity.y*scale};
    return sample;
}
float meshCenterClearRadius(int width,int height);
float meshCenterOpacity(Vec2 position,int width,int height);

class TriangleMesh {
    std::vector<MeshNode> nodes_;
    std::vector<MeshTriangle> triangles_;
    std::vector<MeshEdge> edges_;
    std::vector<uint32_t> free_,newAnchors_;
    std::vector<std::vector<uint32_t>> cells_;
    struct CoverageProbe {Vec2 position;double emptySince=-1;uint8_t motionRegion{};};
    enum class BirthSite { Inlet, Center, Interior };
    std::vector<CoverageProbe> probes_;
    Vec2 origin_{},latticeOrigin_{},translationPhase_{};
    float spacing_{},buffer_{},topologyTime_{},centerClearRadius_=-1;
    double seconds_{};
    bool liveTracking_{};
    bool edgesEnabled_{};
    bool referencePhaseValid_=true,centerFade_=true;
    int width_{},height_{},columns_{},rows_{};
    size_t faceCursor_{},probeCursor_{},nodeLimit_{};
    uint64_t nextId_{};
    MeshStats stats_;

    void indexNode(uint32_t index);
    void rebuildIndex();
    int cellAt(float coordinate,float start,int count) const;
    template<class Visit> void nearby(Vec2 center,float radius,Visit visit) const {
        const int left=cellAt(center.x-radius,origin_.x,columns_),right=cellAt(center.x+radius,origin_.x,columns_);
        const int top=cellAt(center.y-radius,origin_.y,rows_),bottom=cellAt(center.y+radius,origin_.y,rows_);
        for(int row=top;row<=bottom;++row)for(int column=left;column<=right;++column)
            for(auto index:cells_[size_t(row)*columns_+column])visit(index);
    }
    bool add(Vec2 at,const MotionField& field,BirthSite site,bool expanding=false);
    void retire(MeshNode& node);
    bool hasRoom(Vec2 position,float distance,uint8_t motionRegion=0) const;
    bool findAlignedSite(Vec2 target,const MotionField& field,BirthSite site,Vec2& result) const;
    void replenish(const MotionField& field,bool alignedTranslation,float radialRate,float sinkRadius);
    void recycleCrowding(const MotionField& field,float sinkRadius,float elapsed);
    void reconnect();
    bool linkCompatible(Vec2 a,Vec2 b,bool existing=false) const;
    void updateEdges();
    void countNodes();
public:
    explicit TriangleMesh(bool liveTracking=false):liveTracking_(liveTracking),edgesEnabled_(!liveTracking){}
    void seed(int width,int height,float spacing);
    void advance(const MotionField& field,float seconds);
    void stopVelocity();
    void setEdgesEnabled(bool enabled);
    void restartMotionChecks();
    void setCenterFade(bool enabled){centerFade_=enabled;}
    void setCenterClearRadius(float radius){centerClearRadius_=radius;}
    std::span<const MeshNode> nodes() const {return nodes_;}
    std::span<const MeshTriangle> triangles() const {return triangles_;}
    std::span<const MeshEdge> edges() const {return edges_;}
    const MeshStats& stats() const {return stats_;}
};

class LiveTriangleBackend {
    TriangleMesh mesh_{true};
    std::vector<DotVisual> dots_;
    int width_{},height_{};
    float spacing_{},settle_{},updateMs_{};
    // Signed depth strength before applying the screen-position envelope.
    float depthCue_{};
    bool advancing_{};
public:
    void reset();
    void suspend();
    void setEdgesEnabled(bool enabled){mesh_.setEdgesEnabled(enabled);}
    std::span<const MeshNode> nodes() const {return mesh_.nodes();}
    std::span<const MeshEdge> edges() const {return mesh_.edges();}
    const std::vector<DotVisual>& update(int width,int height,const VisualSettings& settings,
                                        const MotionField* source,float elapsed,float signedGain,bool moving,float appearanceElapsed);
    const MeshStats& stats() const {return mesh_.stats();}
    float spacing() const {return spacing_;}
    float updateMs() const {return updateMs_;}
};

}

#pragma once
#include "ModelRuntimeResource.h"

enum class AnimBlendMode {
    None,
    Linear,
    PositionDelta
};

struct AnimPlayState {
    int clipA = -1;
    int clipB = -1;
    double timeA = 0.0;
    double timeB = 0.0;
    double speedA = 1.0;
    double speedB = 1.0;
    bool loopA = true;
    bool loopB = true;
    AnimBlendMode mode = AnimBlendMode::None;
    double blendTime = 0.0;
    double blendDuration = 0.0;
};

class ModelAnimationController {
public:
    void Reset();
    void SetResource(std::shared_ptr<ModelRuntimeResource> res) { m_res = res; }

    bool Play(int clipIndex, bool loop, double speed = 1.0);
    bool BlendTo(int clipIndex, double duration, AnimBlendMode mode, bool loop, double speed = 1.0);
    void Update(double dt);
    void Evaluate();

private:
    void AdvanceClip(double& t, double dt, int clip, double tps, bool loop, double duration);
    void ApplyClip(int clip, double timeInTicks);
    void LerpPoseLinear(float alpha);
    void ApplyPositionDelta(float alpha);

private:
    std::shared_ptr<ModelRuntimeResource> m_res;
    AnimPlayState m_state;
    std::vector<DirectX::XMMATRIX> m_localA;
    std::vector<DirectX::XMMATRIX> m_localB;
};
#include "ModelAnimationController.h"
#include <DirectXMath.h>
#include <algorithm>

using namespace DirectX;

static void InterpVec3(const std::vector<AModelKeyFrame>& keys, double t, XMFLOAT3& out) {
    if (keys.empty()) return;
    if (keys.size() == 1) { out = keys[0].t; return; }
    size_t k = 0; while (k + 1 < keys.size() && keys[k + 1].time < t) k++;
    size_t k2 = (k + 1 < keys.size()) ? k + 1 : k;
    double t0 = keys[k].time, t1 = keys[k2].time;
    double a = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
    out.x = (float)(keys[k].t.x + (keys[k2].t.x - keys[k].t.x) * a);
    out.y = (float)(keys[k].t.y + (keys[k2].t.y - keys[k].t.y) * a);
    out.z = (float)(keys[k].t.z + (keys[k2].t.z - keys[k].t.z) * a);
}
static void InterpQuat(const std::vector<AModelKeyFrame>& keys, double t, XMFLOAT4& out) {
    if (keys.empty()) return;
    if (keys.size() == 1) { out = keys[0].r; return; }
    size_t k = 0; while (k + 1 < keys.size() && keys[k + 1].time < t) k++;
    size_t k2 = (k + 1 < keys.size()) ? k + 1 : k;
    double t0 = keys[k].time, t1 = keys[k2].time;
    double a = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
    XMVECTOR q0 = XMLoadFloat4(&keys[k].r);
    XMVECTOR q1 = XMLoadFloat4(&keys[k2].r);
    XMVECTOR q = XMQuaternionSlerp(q0, q1, (float)a);
    XMStoreFloat4(&out, q);
}

void ModelAnimationController::Reset() {
    m_state = {};
    m_localA.clear();
    m_localB.clear();
}

bool ModelAnimationController::Play(int clipIndex, bool loop, double speed) {
    if (!m_res || clipIndex < 0 || clipIndex >= (int)m_res->animations.size()) return false;
    m_state.clipA = clipIndex;
    m_state.loopA = loop;
    m_state.speedA = speed;
    m_state.timeA = 0.0;
    m_state.mode = AnimBlendMode::None;
    m_state.clipB = -1;
    return true;
}

bool ModelAnimationController::BlendTo(int clipIndex, double duration, AnimBlendMode mode, bool loop, double speed) {
    if (!m_res || clipIndex < 0 || clipIndex >= (int)m_res->animations.size()) return false;
    if (m_state.clipA < 0) {
        return Play(clipIndex, loop, speed);
    }
    m_state.clipB = clipIndex;
    m_state.loopB = loop;
    m_state.speedB = speed;
    m_state.timeB = 0.0;
    m_state.mode = mode;
    m_state.blendDuration = duration;
    m_state.blendTime = 0.0;
    return true;
}

void ModelAnimationController::AdvanceClip(double& t, double dt, int clip, double tps, bool loop, double duration) {
    t += dt * tps;
    if (loop) {
        while (t > duration) t -= duration;
    }
    else if (t > duration) {
        t = duration;
    }
}

void ModelAnimationController::Update(double dt) {
    if (!m_res) return;
    if (m_state.clipA < 0) return;
    auto& A = m_res->animations[m_state.clipA];
    AdvanceClip(m_state.timeA, dt * m_state.speedA, m_state.clipA, A.ticksPerSecond, m_state.loopA, A.duration);

    if (m_state.clipB >= 0) {
        auto& B = m_res->animations[m_state.clipB];
        AdvanceClip(m_state.timeB, dt * m_state.speedB, m_state.clipB, B.ticksPerSecond, m_state.loopB, B.duration);
        if (m_state.blendDuration > 0.0) {
            m_state.blendTime += dt;
            if (m_state.blendTime >= m_state.blendDuration) {
                m_state.clipA = m_state.clipB;
                m_state.timeA = m_state.timeB;
                m_state.speedA = m_state.speedB;
                m_state.loopA = m_state.loopB;
                m_state.clipB = -1;
                m_state.mode = AnimBlendMode::None;
                m_state.blendDuration = 0.0;
                m_state.blendTime = 0.0;
            }
        }
    }
    Evaluate();
}

void ModelAnimationController::ApplyClip(int clip, double timeInTicks) {
    if (!m_res || clip < 0) return;
    auto& bones = m_res->bones;
    auto& anim = m_res->animations[clip];

    for (auto& ch : anim.channels) {
        int bi = m_res->FindBoneIndex(ch.boneName);
        if (bi < 0) continue;
        XMFLOAT3 T{ 0,0,0 }, S{ 1,1,1 }; XMFLOAT4 R{ 0,0,0,1 };
        InterpVec3(ch.translations, timeInTicks, T);
        InterpVec3(ch.scalings, timeInTicks, S);
        InterpQuat(ch.rotations, timeInTicks, R);
        XMVECTOR t = XMLoadFloat3(&T);
        XMVECTOR s = XMLoadFloat3(&S);
        XMVECTOR r = XMLoadFloat4(&R);
        m_res->bones[bi].local = XMMatrixAffineTransformation(s, XMVectorZero(), r, t);
    }
    for (size_t i = 0; i < bones.size(); i++) {
        if (bones[i].parent < 0)
            bones[i].global = bones[i].local;
        else
            bones[i].global = bones[bones[i].parent].global * bones[i].local;
    }
}

void ModelAnimationController::LerpPoseLinear(float alpha) {
    for (size_t i = 0; i < m_res->bones.size(); i++) {
        if (i < m_localA.size() && i < m_localB.size()) {
            XMMATRIX out;
            out.r[0] = XMVectorLerp(m_localA[i].r[0], m_localB[i].r[0], alpha);
            out.r[1] = XMVectorLerp(m_localA[i].r[1], m_localB[i].r[1], alpha);
            out.r[2] = XMVectorLerp(m_localA[i].r[2], m_localB[i].r[2], alpha);
            out.r[3] = XMVectorLerp(m_localA[i].r[3], m_localB[i].r[3], alpha);
            m_res->bones[i].global = out;
        }
    }
}

void ModelAnimationController::ApplyPositionDelta(float alpha) {
    if (m_res->bones.empty()) return;
    if (m_localA.empty() || m_localB.empty()) return;
    XMVECTOR ta = m_localA[0].r[3];
    XMVECTOR tb = m_localB[0].r[3];
    XMVECTOR t = XMVectorLerp(ta, tb, alpha);
    m_res->bones[0].global.r[3] = t;
}

void ModelAnimationController::Evaluate() {
    if (!m_res) return;
    auto& bones = m_res->bones;
    if (bones.empty()) return;

    if (m_state.clipA >= 0) {
        auto& A = m_res->animations[m_state.clipA];
        ApplyClip(m_state.clipA, m_state.timeA);
        m_localA.resize(bones.size());
        for (size_t i = 0; i < bones.size(); i++) m_localA[i] = bones[i].global;
    }
    if (m_state.clipB >= 0) {
        auto& B = m_res->animations[m_state.clipB];
        ApplyClip(m_state.clipB, m_state.timeB);
        m_localB.resize(bones.size());
        for (size_t i = 0; i < bones.size(); i++) m_localB[i] = bones[i].global;
    }

    if (m_state.clipB >= 0 && m_state.mode != AnimBlendMode::None && m_state.blendDuration > 0.0) {
        float alpha = (float)(m_state.blendTime / m_state.blendDuration);
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        switch (m_state.mode) {
        case AnimBlendMode::Linear:       LerpPoseLinear(alpha); break;
        case AnimBlendMode::PositionDelta:ApplyPositionDelta(alpha); break;
        default: break;
        }
    }

    for (size_t i = 0; i < bones.size() && i < AMODEL_MAX_BONES; i++) {
        m_res->finalBonePalette[i] = bones[i].offset * bones[i].global;
    }
}
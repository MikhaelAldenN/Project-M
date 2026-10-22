#include "AnimationController.h"
#include <functional>

using namespace DirectX;

void AnimationController::Initialize(std::shared_ptr<Model> model)
{
    m_ownerModel = model;
    size_t nodeCount = m_ownerModel->GetNodes().size();

    // Current pose and the snapshot used for blending
    m_nodePoses.resize(nodeCount);
    m_prevNodePoses.resize(nodeCount);
}

void AnimationController::Play(const std::string& name, bool loop, float blendTime)
{
    if (!m_ownerModel) return;

    int newIndex = m_ownerModel->GetAnimationIndex(name.c_str());
    if (newIndex == -1) return;

    if (newIndex != m_currentAnimIndex)
    {
        // --- Start a blend ---

        // When another animation was playing (not the initial T-pose),
        // its last pose is kept in prevNodePoses and blended out.
        if (m_currentAnimIndex != -1)
        {
            m_prevNodePoses = m_nodePoses;

            // Setup blending
            m_isBlending = true;
            m_blendDuration = blendTime;
            m_blendTimer = 0.0f;
        }
        else
        {
            // The first animation starts without a blend.
            m_isBlending = false;
        }

        // --- Reset state for the new animation ---
        m_currentAnimIndex = newIndex;
        m_timer = 0.0f;
        m_isLooping = loop;
        m_playbackSpeed = 1.0f;
    }
}

void AnimationController::Update(float dt)
{
    if (!m_ownerModel || m_currentAnimIndex == -1) return;

    // 1. Advance the main animation timer
    const auto& anims = m_ownerModel->GetAnimations();
    float duration = anims.at(m_currentAnimIndex).secondsLength;

    // Guard against division by zero for zero-length clips.
    if (duration > 0.0f)
    {
        m_timer += dt * m_playbackSpeed;

        if (m_isLooping)
        {
            // Wrap the time into [0, duration) without fmod.
            while (m_timer < 0.0f) m_timer += duration;
            while (m_timer >= duration) m_timer -= duration;
        }
        else
        {
            // Clamp strictly
            if (m_timer < 0.0f) m_timer = 0.0f;
            if (m_timer > duration) m_timer = duration;
        }
    }
    else
    {
        m_timer = 0.0f; // Safe fallback for 0-second animations
    }

    // 2. Pose of the new animation, written to nodePoses first
    m_ownerModel->ComputeAnimation(m_currentAnimIndex, m_timer, m_nodePoses);

    // 3. Blend from the snapshot, while a blend runs
    if (m_isBlending)
    {
        m_blendTimer += dt;

        // blend weight, 0 to 1
        float t = m_blendTimer / m_blendDuration;

        if (t >= 1.0f)
        {
            t = 1.0f;
            m_isBlending = false; // blend finished
        }

        // Per node (bone)
        size_t count = m_nodePoses.size();
        for (size_t i = 0; i < count; ++i)
        {
            // Read from the previous pose snapshot.
            XMVECTOR s0 = XMLoadFloat3(&m_prevNodePoses[i].scale);
            XMVECTOR r0 = XMLoadFloat4(&m_prevNodePoses[i].rotation);
            XMVECTOR t0 = XMLoadFloat3(&m_prevNodePoses[i].position);

            // New pose (target)
            XMVECTOR s1 = XMLoadFloat3(&m_nodePoses[i].scale);
            XMVECTOR r1 = XMLoadFloat4(&m_nodePoses[i].rotation);
            XMVECTOR t1 = XMLoadFloat3(&m_nodePoses[i].position);

            // Lerp for scale and position
            XMVECTOR sFinal = XMVectorLerp(s0, s1, t);
            XMVECTOR tFinal = XMVectorLerp(t0, t1, t);
            // Slerp for rotation, so the turn stays smooth
            XMVECTOR rFinal = XMQuaternionSlerp(r0, r1, t);

            // Result goes back into nodePoses, which is what gets drawn
            XMStoreFloat3(&m_nodePoses[i].scale, sFinal);
            XMStoreFloat4(&m_nodePoses[i].rotation, rFinal);
            XMStoreFloat3(&m_nodePoses[i].position, tFinal);
        }
    }

    // 4. Upper-body layer
    if (m_upperAnimIndex != -1 && !m_upperBodyMask.empty())
    {
        m_upperTimer += dt;
        float upperDuration = anims.at(m_upperAnimIndex).secondsLength;

        // upper-body animation reached its end
        if (m_upperTimer >= upperDuration)
        {
            if (m_upperIsLooping) {
                m_upperTimer = fmod(m_upperTimer, upperDuration); // loop
            }
            else {
                m_upperAnimIndex = -1; // stop the upper-body animation
            }
        }

        // Still playing
        if (m_upperAnimIndex != -1)
        {
            if (m_upperNodePoses.size() != m_ownerModel->GetNodes().size()) {
                m_upperNodePoses.resize(m_ownerModel->GetNodes().size());
            }
            m_ownerModel->ComputeAnimation(m_upperAnimIndex, m_upperTimer, m_upperNodePoses);

            // --- BONE MASKING BLEND ---
            for (size_t i = 0; i < m_nodePoses.size(); ++i)
            {
                if (m_upperBodyMask[i])
                {
                    // masked nodes take the upper-body pose
                    m_nodePoses[i] = m_upperNodePoses[i];
                }
            }
        }
    }
    // 5. Upload the pose to the model
    m_ownerModel->SetNodePoses(m_nodePoses);
}

bool AnimationController::IsPlaying(const std::string& name) const
{
    if (!m_ownerModel) return false;
    return m_currentAnimIndex == m_ownerModel->GetAnimationIndex(name.c_str());
}

void AnimationController::SetUpperBodyMaskRoot(const std::string& rootNodeName)
{
    if (!m_ownerModel) return;

    m_upperBodyMask.assign(m_ownerModel->GetNodes().size(), false);
    int splitIndex = m_ownerModel->GetNodeIndex(rootNodeName.c_str());
    if (splitIndex == -1) return;

    // Recursive lambda: marks the root node and every node below it
    std::function<void(Model::Node*)> markNode = [&](Model::Node* node) {
        int idx = m_ownerModel->GetNodeIndex(node->name.c_str());
        if (idx != -1) m_upperBodyMask[idx] = true;

        for (Model::Node* child : node->children) {
            markNode(child);
        }
        };

    markNode(&m_ownerModel->GetNodes()[splitIndex]);
}

void AnimationController::PlayUpper(const std::string& name, bool loop)
{
    if (!m_ownerModel) return;
    m_upperAnimIndex = m_ownerModel->GetAnimationIndex(name.c_str());
    m_upperTimer = 0.0f;
    m_upperIsLooping = loop;
}
#include "AnimationController.h"
#include <functional>

using namespace DirectX;

void AnimationController::Initialize(std::shared_ptr<Model> model)
{
    ownerModel = model;
    size_t nodeCount = ownerModel->GetNodes().size();

    // Current pose and the snapshot used for blending
    nodePoses.resize(nodeCount);
    prevNodePoses.resize(nodeCount);
}

void AnimationController::Play(const std::string& name, bool loop, float blendTime)
{
    if (!ownerModel) return;

    int newIndex = ownerModel->GetAnimationIndex(name.c_str());
    if (newIndex == -1) return;

    if (newIndex != currentAnimIndex)
    {
        // --- Start a blend ---

        // When another animation was playing (not the initial T-pose),
        // its last pose is kept in prevNodePoses and blended out.
        if (currentAnimIndex != -1)
        {
            prevNodePoses = nodePoses;

            // Setup blending
            isBlending = true;
            blendDuration = blendTime;
            blendTimer = 0.0f;
        }
        else
        {
            // The first animation starts without a blend.
            isBlending = false;
        }

        // --- Reset state for the new animation ---
        currentAnimIndex = newIndex;
        timer = 0.0f;
        isLooping = loop;
        m_playbackSpeed = 1.0f;
    }
}

void AnimationController::Update(float dt)
{
    if (!ownerModel || currentAnimIndex == -1) return;

    // 1. Advance the main animation timer
    const auto& anims = ownerModel->GetAnimations();
    float duration = anims.at(currentAnimIndex).secondsLength;

    // Guard against division by zero for zero-length clips.
    if (duration > 0.0f)
    {
        timer += dt * m_playbackSpeed;

        if (isLooping)
        {
            // Wrap the time into [0, duration) without fmod.
            while (timer < 0.0f) timer += duration;
            while (timer >= duration) timer -= duration;
        }
        else
        {
            // Clamp strictly
            if (timer < 0.0f) timer = 0.0f;
            if (timer > duration) timer = duration;
        }
    }
    else
    {
        timer = 0.0f; // Safe fallback for 0-second animations
    }

    // 2. Pose of the new animation, written to nodePoses first
    ownerModel->ComputeAnimation(currentAnimIndex, timer, nodePoses);

    // 3. Blend from the snapshot, while a blend runs
    if (isBlending)
    {
        blendTimer += dt;

        // blend weight, 0 to 1
        float t = blendTimer / blendDuration;

        if (t >= 1.0f)
        {
            t = 1.0f;
            isBlending = false; // blend finished
        }

        // Per node (bone)
        size_t count = nodePoses.size();
        for (size_t i = 0; i < count; ++i)
        {
            // Read from the previous pose snapshot.
            XMVECTOR s0 = XMLoadFloat3(&prevNodePoses[i].scale);
            XMVECTOR r0 = XMLoadFloat4(&prevNodePoses[i].rotation);
            XMVECTOR t0 = XMLoadFloat3(&prevNodePoses[i].position);

            // New pose (target)
            XMVECTOR s1 = XMLoadFloat3(&nodePoses[i].scale);
            XMVECTOR r1 = XMLoadFloat4(&nodePoses[i].rotation);
            XMVECTOR t1 = XMLoadFloat3(&nodePoses[i].position);

            // Lerp for scale and position
            XMVECTOR sFinal = XMVectorLerp(s0, s1, t);
            XMVECTOR tFinal = XMVectorLerp(t0, t1, t);
            // Slerp for rotation, so the turn stays smooth
            XMVECTOR rFinal = XMQuaternionSlerp(r0, r1, t);

            // Result goes back into nodePoses, which is what gets drawn
            XMStoreFloat3(&nodePoses[i].scale, sFinal);
            XMStoreFloat4(&nodePoses[i].rotation, rFinal);
            XMStoreFloat3(&nodePoses[i].position, tFinal);
        }
    }

    // 4. Upper-body layer
    if (upperAnimIndex != -1 && !upperBodyMask.empty())
    {
        upperTimer += dt;
        float upperDuration = anims.at(upperAnimIndex).secondsLength;

        // upper-body animation reached its end
        if (upperTimer >= upperDuration)
        {
            if (upperIsLooping) {
                upperTimer = fmod(upperTimer, upperDuration); // loop
            }
            else {
                upperAnimIndex = -1; // stop the upper-body animation
            }
        }

        // Still playing
        if (upperAnimIndex != -1)
        {
            if (upperNodePoses.size() != ownerModel->GetNodes().size()) {
                upperNodePoses.resize(ownerModel->GetNodes().size());
            }
            ownerModel->ComputeAnimation(upperAnimIndex, upperTimer, upperNodePoses);

            // --- BONE MASKING BLEND ---
            for (size_t i = 0; i < nodePoses.size(); ++i)
            {
                if (upperBodyMask[i])
                {
                    // masked nodes take the upper-body pose
                    nodePoses[i] = upperNodePoses[i];
                }
            }
        }
    }
    // 5. Upload the pose to the model
    ownerModel->SetNodePoses(nodePoses);
}

bool AnimationController::IsPlaying(const std::string& name) const
{
    if (!ownerModel) return false;
    return currentAnimIndex == ownerModel->GetAnimationIndex(name.c_str());
}

void AnimationController::SetUpperBodyMaskRoot(const std::string& rootNodeName)
{
    if (!ownerModel) return;

    upperBodyMask.assign(ownerModel->GetNodes().size(), false);
    int splitIndex = ownerModel->GetNodeIndex(rootNodeName.c_str());
    if (splitIndex == -1) return;

    // Recursive lambda: marks the root node and every node below it
    std::function<void(Model::Node*)> markNode = [&](Model::Node* node) {
        int idx = ownerModel->GetNodeIndex(node->name.c_str());
        if (idx != -1) upperBodyMask[idx] = true;

        for (Model::Node* child : node->children) {
            markNode(child);
        }
        };

    markNode(&ownerModel->GetNodes()[splitIndex]);
}

void AnimationController::PlayUpper(const std::string& name, bool loop)
{
    if (!ownerModel) return;
    upperAnimIndex = ownerModel->GetAnimationIndex(name.c_str());
    upperTimer = 0.0f;
    upperIsLooping = loop;
}
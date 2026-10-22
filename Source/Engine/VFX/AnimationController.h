#pragma once
#include <vector>
#include <string>
#include <memory>
#include <DirectXMath.h>
#include "System/Model.h"

class AnimationController
{
public:
    void Initialize(std::shared_ptr<Model> model);
    void Update(float dt);

    // blendTime defaults to 0.2 s
    void Play(const std::string& name, bool loop = true, float blendTime = 0.2f);

    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    [[nodiscard]] float GetPlaybackSpeed() const { return m_playbackSpeed; }

    bool IsPlaying(const std::string& name) const;
    float GetCurrentTime() const { return timer; }

    // Builds the upper-body mask from rootNodeName and every node below it.
    void SetUpperBodyMaskRoot(const std::string& rootNodeName);

    // Plays an animation on the masked upper-body nodes only.
    void PlayUpper(const std::string& name, bool loop = false);

    // True while an upper-body animation is playing.
    bool IsUpperPlaying() const { return upperAnimIndex != -1; }

    // Whether the current upper-body animation loops.
    bool upperIsLooping = false;

private:
    std::shared_ptr<Model> ownerModel;

    // Current animation
    int currentAnimIndex = -1;
    float timer = 0.0f;
    bool isLooping = true;
    float m_playbackSpeed{ 1.0f };
    int upperAnimIndex = -1;
    float upperTimer = 0.0f;

    // Blend state
    bool isBlending = false;
    float blendTimer = 0.0f;
    float blendDuration = 0.0f;

    // Pose buffers
    // final pose sent to the model
    std::vector<Model::NodePose> nodePoses;

    // last pose of the previous animation, blended out
    std::vector<Model::NodePose> prevNodePoses;

    std::vector<Model::NodePose> upperNodePoses;
    std::vector<bool> upperBodyMask;
};
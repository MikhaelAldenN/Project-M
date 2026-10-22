#include "Camera.h"
#include <cmath>

using namespace DirectX;

Camera::Camera() :
    m_position({ 0, 0, -10 }),
    m_rotation({ 0, 0, 0 }),
    m_front({ 0, 0, 1 }),
    m_right({ 1, 0, 0 }),
    m_up({ 0, 1, 0 })
{
    XMStoreFloat4x4(&m_view, XMMatrixIdentity());
    XMStoreFloat4x4(&m_projection, XMMatrixIdentity());
    UpdateViewMatrix();
}

Camera::~Camera() {}

// --- Transform Operations ---

void Camera::SetPosition(const XMFLOAT3& pos)
{
    this->m_position = pos;
    UpdateViewMatrix();
}

void Camera::SetPosition(float x, float y, float z)
{
    SetPosition({ x, y, z });
}

void Camera::Translate(const XMFLOAT3& translation)
{
    XMVECTOR vPos = XMLoadFloat3(&m_position);
    XMVECTOR vFront = XMLoadFloat3(&m_front);
    XMVECTOR vRight = XMLoadFloat3(&m_right);
    XMVECTOR vUp = XMLoadFloat3(&m_up);

    vPos = XMVectorAdd(vPos, XMVectorScale(vRight, translation.x));
    vPos = XMVectorAdd(vPos, XMVectorScale(vUp, translation.y));
    vPos = XMVectorAdd(vPos, XMVectorScale(vFront, translation.z));

    XMStoreFloat3(&m_position, vPos);
    UpdateViewMatrix();
}

void Camera::Translate(float x, float y, float z)
{
    Translate({ x, y, z });
}

void Camera::SetRotation(const XMFLOAT3& rot)
{
    this->m_rotation = rot;

    // Clamp Pitch to prevent gimbal lock
    constexpr float limit = XMConvertToRadians(90.0f);
    if (this->m_rotation.x > limit) this->m_rotation.x = limit;
    if (this->m_rotation.x < -limit) this->m_rotation.x = -limit;

    UpdateViewMatrix();
}

void Camera::SetRotation(float x, float y, float z)
{
    SetRotation({ x, y, z });
}

void Camera::LookAt(const XMFLOAT3& target)
{
    XMVECTOR vPos = XMLoadFloat3(&m_position);
    XMVECTOR vTarget = XMLoadFloat3(&target);
    XMVECTOR vDir = XMVectorSubtract(vTarget, vPos);
    vDir = XMVector3Normalize(vDir);

    XMFLOAT3 dir;
    XMStoreFloat3(&dir, vDir);

    float pitch = asinf(-dir.y);
    float yaw = atan2f(dir.x, dir.z);

    SetRotation({ pitch, yaw, 0.0f });
}

void Camera::SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up)
{
    this->m_position = eye;
    LookAt(focus);
}

// --- Projection Settings ---

void Camera::SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
{
    this->m_fovY = fovY;
    this->m_nearZ = nearZ;
    this->m_farZ = farZ;

    XMMATRIX matProj = XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
    XMStoreFloat4x4(&m_projection, matProj);
}

void Camera::SetAspectRatio(float aspect)
{
    SetPerspectiveFov(this->m_fovY, aspect, this->m_nearZ, this->m_farZ);
}

void Camera::SetOffCenterProjection(float left, float right, float bottom, float top, float nZ, float fZ)
{
    this->m_nearZ = nZ;
    this->m_farZ = fZ;
    XMMATRIX matProj = XMMatrixPerspectiveOffCenterLH(left, right, bottom, top, nZ, fZ);
    XMStoreFloat4x4(&m_projection, matProj);
}

void Camera::SetOrthographic(float viewWidth, float viewHeight, float nearZ, float farZ)
{
    this->m_nearZ = nearZ;
    this->m_farZ = farZ;
    // Orthographic projection: a box instead of a frustum
    XMMATRIX matProj = XMMatrixOrthographicLH(viewWidth, viewHeight, nearZ, farZ);
    XMStoreFloat4x4(&m_projection, matProj);
}

// --- Helpers & Internals ---

DirectX::XMFLOAT3 Camera::GetFocus() const
{
    return { m_position.x + m_front.x, m_position.y + m_front.y, m_position.z + m_front.z };
}

void Camera::UpdateViewMatrix()
{
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);

    XMVECTOR vFront = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), matRot);
    XMVECTOR vRight = XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), matRot);
    XMVECTOR vUp = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), matRot);

    XMStoreFloat3(&m_front, vFront);
    XMStoreFloat3(&m_right, vRight);
    XMStoreFloat3(&m_up, vUp);

    XMVECTOR vPos = XMLoadFloat3(&m_position);
    XMStoreFloat4x4(&m_view, XMMatrixLookToLH(vPos, vFront, vUp));
}

bool Camera::CheckSphere(float x, float y, float z, float radius)
{
    // 1. View * projection
    XMMATRIX matView = XMLoadFloat4x4(&m_view);
    XMMATRIX matProj = XMLoadFloat4x4(&m_projection);
    XMMATRIX matViewProj = XMMatrixMultiply(matView, matProj);

    XMFLOAT4X4 M;
    XMStoreFloat4x4(&M, matViewProj);

    // 2. The six frustum planes (left, right, bottom, top, near, far)
    // Gribb-Hartmann: each plane is column 4 plus or minus column X
    float planes[6][4];

    // Left
    planes[0][0] = M._14 + M._11; planes[0][1] = M._24 + M._21; planes[0][2] = M._34 + M._31; planes[0][3] = M._44 + M._41;
    // Right
    planes[1][0] = M._14 - M._11; planes[1][1] = M._24 - M._21; planes[1][2] = M._34 - M._31; planes[1][3] = M._44 - M._41;
    // Bottom
    planes[2][0] = M._14 + M._12; planes[2][1] = M._24 + M._22; planes[2][2] = M._34 + M._32; planes[2][3] = M._44 + M._42;
    // Top
    planes[3][0] = M._14 - M._12; planes[3][1] = M._24 - M._22; planes[3][2] = M._34 - M._32; planes[3][3] = M._44 - M._42;
    // Near
    planes[4][0] = M._13;         planes[4][1] = M._23;         planes[4][2] = M._33;         planes[4][3] = M._43;
    // Far
    planes[5][0] = M._14 - M._13; planes[5][1] = M._24 - M._23; planes[5][2] = M._34 - M._33; planes[5][3] = M._44 - M._43;

    // 3. Sphere against each plane
    for (int i = 0; i < 6; ++i)
    {
        // Normalized, so the distance is in world units
        float length = sqrtf(planes[i][0] * planes[i][0] + planes[i][1] * planes[i][1] + planes[i][2] * planes[i][2]);

        // Signed distance from the point to the plane
        float dist = (planes[i][0] * x + planes[i][1] * y + planes[i][2] * z + planes[i][3]) / length;

        // Below -radius the sphere lies entirely behind this plane (off screen).
        if (dist < -radius)
        {
            return false; // outside: cull
        }
    }

    return true; // Visible
}
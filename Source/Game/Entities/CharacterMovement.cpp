#include "CharacterMovement.h"
#include <cmath>

bool CharacterMovement::IsMoving() const
{
    return (fabsf(m_velocity.x) > 0.01f || fabsf(m_velocity.z) > 0.01f);
}
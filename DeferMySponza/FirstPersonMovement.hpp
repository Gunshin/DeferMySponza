/*
 @file      FirstPersonMovement.hpp
 @author    Tyrone Davison
 @date      October 2012
 */

#pragma once
#ifndef __FIRSTPERSONMOVEMENT__
#define __FIRSTPERSONMOVEMENT__

#include <glm/glm.hpp>

/**
 Provides position and direction from strafe and rotate commands.
 */
class FirstPersonMovement
{
public:

    FirstPersonMovement() : dirty_(true)
    {
    }

    void
    init(glm::vec3 position,
         float azimuth,
         float inclination)
    {
        position_ = position;
        azimuth_radians_ = azimuth;
        inclination_radians_ = inclination;
        dirty_ = true;
    }

    glm::vec3 position()
    {
        return position_;
    }

    glm::vec3 direction()
    {
        return forward();
    }

    float azimuth()
    {
        return azimuth_radians_;
    }

    float inclination()
    {
        return inclination_radians_;
    }

    void moveForward(float distance)
    {
        position_ += forward() * distance;
    }

    void moveRight(float distance)
    {
        position_ += right() * distance;
    }

    void spinHorizontal(float angle_radians)
    {
        azimuth_radians_ += angle_radians;
        dirty_ = true;
    }

    void spinVertical(float angle_radians)
    {
        inclination_radians_ += angle_radians;
        dirty_ = true;
    }

private:
    glm::vec3 position_;
    glm::vec3 forward_;
    glm::vec3 right_;
    float azimuth_radians_;
    float inclination_radians_;
    bool dirty_;

    void update()
    {
        const float azi = azimuth_radians_;
        const float inc = inclination_radians_;
        const float cos_azi = cosf(azi);
        const float sin_azi = sinf(azi);
        const float cos_inc = cosf(inc);
        const float sin_inc = sinf(inc);
        forward_ = glm::vec3(cos_inc * -sin_azi, sin_inc, cos_inc * -cos_azi);
        right_ = glm::vec3(cos_azi, 0, -sin_azi);
        dirty_ = false;
    }

    glm::vec3 forward()
    {
        if (dirty_) {
            update();
        }
        return forward_;
    }

    glm::vec3 right()
    {
        if (dirty_) {
            update();
        }
        return right_;
    }

};

#endif

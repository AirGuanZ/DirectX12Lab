#pragma once

#include <agz/d3d12/common.h>

AGZ_D3D12_BEGIN

class WalkingCamera
{
public:

    struct UpdateParams
    {
        float rotateLeft = 0;
        float rotateDown = 0;

        bool forward  = false;
        bool backward = false;
        bool left     = false;
        bool right    = false;

        bool seperateUpDown = false;
        bool up             = false;
        bool down           = false;
    };

    WalkingCamera();

    void setSpeed(float speed) noexcept;

    void setDeadZone(float deadZoneRad) noexcept;

    void setClipZ(float nearZ, float farZ);

    void setWOverH(float wOverH) noexcept;

    void setYDeg(float deg) noexcept;

    void setPosition(const Vec3 &position) noexcept;

    void setDirection(const Vec3 &direction) noexcept;

    void setDirection(float horiRad, float vertRad) noexcept;

    void setLookAt(const Vec3 &lookAt) noexcept;

    void setUp(const Vec3 &up) noexcept;

    void update(UpdateParams &params, float dt) noexcept;

    const Mat4 &getView() const noexcept;

    const Mat4 &getProj() const noexcept;

    const Mat4 &getViewProj() const noexcept;

private:

    float updateVertAngleWithDeadZone(float oldVertRad) const noexcept;

    Vec2 dirToRad(const Vec3 &dir) const noexcept;

    Vec3 radToDir(const Vec2 &rad) const noexcept;

    void updateMatrix() noexcept;

    Vec3 pos_;
    Vec3 dir_;
    Vec3 up_;

    float speed_;
    float deadZoneRad_;

    float wOverH_;
    float yDeg_;

    float nearZ_;
    float farZ_;

    Mat4 view_;
    Mat4 proj_;
    Mat4 viewProj_;
};

inline WalkingCamera::WalkingCamera()
{
    pos_ = { 4, 0, 0 };
    dir_ = { -1, 0, 0 };
    up_  = { 0, 1, 0 };

    speed_       = 1.0f;
    deadZoneRad_ = 0.01f;

    wOverH_ = 1;
    yDeg_   = 60;

    nearZ_ = 0.1f;
    farZ_  = 100.0f;

    setLookAt({ 0, 0, 0 });
    updateMatrix();
}

inline void WalkingCamera::setSpeed(float speed) noexcept
{
    speed_ = speed;
}

inline void WalkingCamera::setDeadZone(float deadZoneRad) noexcept
{
    deadZoneRad_ = deadZoneRad;

    // update dir to avoid dead zone

    const Vec2 rad = dirToRad(dir_);
    const float newRadY = updateVertAngleWithDeadZone(rad.y);
    if(newRadY != rad.y)
    {
        dir_ = radToDir({ rad.x, newRadY });
        updateMatrix();
    }
}

inline void WalkingCamera::setClipZ(float nearZ, float farZ)
{
    nearZ_ = nearZ;
    farZ_ = farZ;
    updateMatrix();
}

inline void WalkingCamera::setWOverH(float wOverH) noexcept
{
    wOverH_ = wOverH;
    updateMatrix();
}

inline void WalkingCamera::setYDeg(float deg) noexcept
{
    yDeg_ = deg;
    updateMatrix();
}

inline void WalkingCamera::setPosition(const Vec3 &position) noexcept
{
    pos_ = position;
    updateMatrix();
}

inline void WalkingCamera::setDirection(const Vec3 &direction) noexcept
{
    const Vec2 rad = dirToRad(direction);
    setDirection(rad.x, rad.y);
}

inline void WalkingCamera::setDirection(float horiRad, float vertRad) noexcept
{
    dir_ = radToDir({ horiRad, updateVertAngleWithDeadZone(vertRad) });
    updateMatrix();
}

inline void WalkingCamera::setLookAt(const Vec3 &lookAt) noexcept
{
    setDirection(lookAt - pos_);
}

inline void WalkingCamera::setUp(const Vec3 &up) noexcept
{
    // save lookat
    const Vec3 lookAt = pos_ + dir_;

    // make sure up != dir_
    const Vec2 rad = dirToRad(dir_);
    dir_ = radToDir({ rad.x, 0.5f * rad.y });

    // update up
    up_ = up.normalize();

    // keep lookat unchanged
    setLookAt(lookAt);
}

inline void WalkingCamera::update(UpdateParams &params, float dt) noexcept
{
    // update direction

    const Vec2 newRad = dirToRad(dir_) - Vec2(params.rotateLeft, params.rotateDown);
    setDirection(newRad.x, newRad.y);

    // update position

    if(params.seperateUpDown)
    {
        const int fwdStep  = params.forward - params.backward;
        const int leftStep = params.left    - params.right;
        const int upStep   = params.up      - params.down;

        if(fwdStep || leftStep || upStep)
        {
            const Vec3 leftDir = cross(up_, dir_).normalize();
            const Vec3 fwdDir  = cross(leftDir, up_);
            const Vec3 moveDir = (float(fwdStep)  * fwdDir  +
                                  float(leftStep) * -leftDir +
                                  float(upStep)   * up_).normalize();

            pos_ += moveDir * speed_ * dt;
        }
    }
    else
    {
        const int fwdStep  = params.forward - params.backward;
        const int leftStep = params.left    - params.right;

        if(fwdStep || leftStep)
        {
            const Vec3 leftDir = cross(up_, dir_).normalize();
            const Vec3 moveDir = (float(fwdStep)  * dir_ +
                                  float(leftStep) * -leftDir).normalize();

            pos_ += moveDir * speed_ * dt;
        }
    }

    updateMatrix();
}

inline const Mat4 &WalkingCamera::getView() const noexcept
{
    return view_;
}

inline const Mat4 &WalkingCamera::getProj() const noexcept
{
    return proj_;
}

inline const Mat4 &WalkingCamera::getViewProj() const noexcept
{
    return viewProj_;
}

inline float WalkingCamera::updateVertAngleWithDeadZone(
    float oldVertRad) const noexcept
{
    constexpr float PI_2 = 0.5f * math::PI_f;
    return math::clamp(oldVertRad, -PI_2 + deadZoneRad_, PI_2 - deadZoneRad_);
}

inline Vec2 WalkingCamera::dirToRad(const Vec3 &dir) const noexcept
{
    const Vec3 left = cross(up_, dir_).normalize();
    const Vec3 fwd  = cross(left, up_);

    const float x = dot(dir, fwd);
    const float y = dot(dir, up_);
    const float z = dot(dir, left);

    const float radX = x == 0 && z == 0 ? 0.0f : std::atan2(z, x);
    const float radY = std::asin(math::clamp(y, -1.0f, 1.0f));

    return { radX, radY };
}

inline Vec3 WalkingCamera::radToDir(const Vec2 &rad) const noexcept
{
    const float y = std::sin(rad.y);
    const float x = std::cos(rad.y) * std::cos(rad.x);
    const float z = std::cos(rad.y) * std::sin(rad.x);
    
    const Vec3 left = cross(up_, dir_).normalize();
    const Vec3 fwd  = cross(left, up_);

    return (x * fwd + y * up_ + z * left).normalize();
}

inline void WalkingCamera::updateMatrix() noexcept
{
    view_ = Trans4::look_at(pos_, pos_ + dir_, up_);
    proj_ = Trans4::perspective(math::deg2rad(yDeg_), wOverH_, nearZ_, farZ_);
    viewProj_ = view_ * proj_;
}

AGZ_D3D12_END

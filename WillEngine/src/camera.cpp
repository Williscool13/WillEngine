#include <camera.h>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::update()
{
    glm::mat4 cameraRotation = getRotationMatrix();
    position += glm::vec3(
        cameraRotation 
        * glm::vec4(
            velocity * 15.0f
            , 0.f
        )
    );
}


void Camera::processSDLEvent(bool inFocus)
{
    if (!inFocus) { velocity = glm::vec3(0.f); return; }

    velocity.x =
        InputManager::Get().isKeyDown(InputManager::Key::A) ? -1 : 0
        + InputManager::Get().isKeyDown(InputManager::Key::D) ? 1 : 0;
    velocity.z =
        InputManager::Get().isKeyDown(InputManager::Key::W) ? -1 : 0
		+ InputManager::Get().isKeyDown(InputManager::Key::S) ? 1 : 0;
    velocity *= TimeUtil::Get().getDeltaTime();
    yaw += InputManager::Get().getMouseXRel() / 200.0f;
    pitch -= InputManager::Get().getMouseYRel() / 200.0f;
    pitch = glm::clamp(pitch, -glm::half_pi<float>(), glm::half_pi<float>());

}

glm::mat4 Camera::getViewMatrix()
{
    // to create a correct model view, we need to move the world in opposite
    // direction to the camera
    //  so we will create the camera model matrix and invert
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix

    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

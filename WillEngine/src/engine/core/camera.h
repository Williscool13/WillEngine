#pragma once
#include <vk_types.h>
#include "will_engine.h"

class Camera {
public:
    glm::vec3 velocity;
    glm::vec3 position;
    // vertical rotation
    float pitch{ 0.f };
    // horizontal rotation
    float yaw{ 0.f };

    glm::mat4 getViewMatrix() const;
    glm::mat4 getRotationMatrix() const;

    void processSDLEvent(bool inFocus);

    glm::vec3 getViewDirection() const;

    void update();
};


class OrbitCamera {
public:
    glm::vec3 position;
	glm::vec3 target;
	glm::vec3 up{ 0.f, 1.f, 0.f };

	float pitch{ 0.f };
	float yaw{ 0.f };

	float distance{ 10.f };

	glm::mat4 getViewMatrix();
	void processSDLEvent(bool inFocus);
	void update();
};
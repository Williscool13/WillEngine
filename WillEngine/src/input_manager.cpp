#include <input_manager.h>
#include <iostream>

InputManager InputManager::instance;

InputManager& InputManager::Get()
{
	return instance;
}

bool InputManager::isKeyPressed(Key key)
{
	return keyStateData[(int)key].pressed;
}

bool InputManager::isKeyReleased(Key key)
{
	return keyStateData[(int)key].released;
}

bool InputManager::isKeyDown(Key key)
{
	return (bool)keyStateData[(int)key].state;
}

bool InputManager::isMousePressed(MouseKey key)
{
	return (bool)mouseStateData[(int)key].pressed;
}

bool InputManager::isMouseReleased(MouseKey key)
{
	return (bool)mouseStateData[(int)key].released;
}

bool InputManager::isMouseDown(MouseKey key)
{
	return (bool)mouseStateData[(int)key].state;
}

void InputManager::init() {
	for (int i = 0; i < keyCount; i++) {
		keyStateData[i].state = KeyState::UP;
		keyStateData[i].pressed = false;
		keyStateData[i].released = false;
	}
}

void InputManager::frame_reset() {
	for (int i = 0; i < keyCount; i++) {
		keyStateData[i].pressed = false;
		keyStateData[i].released = false;
	}

	for (int i = 0; i < mouseButtonCount; i++) {
		mouseStateData[i].pressed = false;
		mouseStateData[i].released = false;
	}

	mouseMotionData.xrel = 0;
	mouseMotionData.yrel = 0;
}

void InputManager::update(const SDL_Event* e) {
// Mouse
	if (e->type == SDL_MOUSEMOTION) {
		mouseMotionData.xrel += (float)e->motion.xrel;
		mouseMotionData.yrel += (float)e->motion.yrel;
		mouseMotionData.x = e->motion.x;
		mouseMotionData.y = e->motion.y;
	}
	else if (e->type == SDL_MOUSEBUTTONDOWN) {
		for (int i = 0; i < mouseButtonCount; i++) {
			if (mouseButtonCodes[i] == e->button.button) {
				if (mouseStateData[mouseButtonCodes[i] - 1].state == MouseState::UP) { 
					mouseStateData[mouseButtonCodes[i] - 1].pressed = true; 
				}
				mouseStateData[mouseButtonCodes[i] - 1].state = MouseState::DOWN;
			}
		}
	}
	
	else if (e->type == SDL_MOUSEBUTTONUP) {
		for (int i = 0; i < mouseButtonCount; i++) {
			if (mouseButtonCodes[i] == e->button.button) {
				if (mouseStateData[mouseButtonCodes[i] - 1].state == MouseState::DOWN) {
					mouseStateData[mouseButtonCodes[i] - 1].released = true; 
				}
				mouseStateData[mouseButtonCodes[i] - 1].state = MouseState::UP;
			}
		}
	}
// Keyboard
	else if (e->type == SDL_KEYDOWN) {
		for (int i = 0; i < keyCount; i++) {
			if (keyCodes[i] == e->key.keysym.sym) {
				if (keyStateData[i].state == KeyState::UP) {
					keyStateData[i].pressed = true;
				}
				keyStateData[i].state = KeyState::DOWN;
				break;
			}
		}
		
	}
	else if (e->type == SDL_KEYUP) {
		for (int i = 0; i < keyCount; i++) {
			if (keyCodes[i] == e->key.keysym.sym) { 
				if (keyStateData[i].state == KeyState::DOWN) { 
					keyStateData[i].released = true; 
				}
				keyStateData[i].state = KeyState::UP;
				break; 
			}
		}

	}
}




float InputManager::getMouseX()
{
	return mouseMotionData.x;
}

float InputManager::getMouseY()
{
	return mouseMotionData.y;
}

float InputManager::getMouseXRel()
{
	return mouseMotionData.xrel;
}

float InputManager::getMouseYRel()
{
	return mouseMotionData.yrel;
}

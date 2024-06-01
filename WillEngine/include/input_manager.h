#pragma once
#include <SDL2/SDL.h>
#include <unordered_map>




class InputManager {
public:
	static InputManager& Get();
	
	

	enum class Key : int { // 8 keys
		ESCAPE = 0,
		W = 1,
		A = 2,
		S = 3,
		D = 4,
		SPACE = 5,
		SHIFT = 6,
		ENTER = 7,
	};

	enum class MouseKey : int { // 3 keys
		LEFT = 0,
		MIDDLE = 1,
		RIGHT = 2,
	};


	bool isKeyPressed(Key key);
	bool isKeyReleased(Key key);
	bool isKeyDown(Key key);

	bool isMousePressed(MouseKey key);
	bool isMouseReleased(MouseKey key);
	bool isMouseDown(MouseKey key);

	float getMouseX();
	float getMouseY();
	float getMouseXRel();
	float getMouseYRel();

	void init();
	void frame_reset();
	void update(const SDL_Event* e);
private:
	static InputManager instance;

	// Keyboard
	enum class KeyState : bool {
		UP = false,
		DOWN = true
	};
	struct KeyStateData {
		KeyState state;
		bool pressed;
		bool released;
	};
	const SDL_Keycode keyCodes[8] = {
		SDLK_ESCAPE,
		SDLK_w,
		SDLK_a,
		SDLK_s,
		SDLK_d,
		SDLK_SPACE,
		SDLK_LSHIFT,
		SDLK_RETURN,
	};
	const int keyCount = 8;
	KeyStateData keyStateData[8];

	// Mouse
	enum class MouseState : bool {
		UP = false,
		DOWN = true
	};
	struct MouseStateData {
		MouseState state;
		bool pressed;
		bool released;
	};
	const 

	struct MouseMotionData {
		float x;
		float y;
		float xrel;
		float yrel;
	};
	int mouseButtonCodes[3] = {
		SDL_BUTTON_LEFT, // 1
		SDL_BUTTON_MIDDLE, // 2
		SDL_BUTTON_RIGHT // 3
	};
	const int mouseButtonCount = 3;
	MouseStateData mouseStateData[3];
	MouseMotionData mouseMotionData;
};


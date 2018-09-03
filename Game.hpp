#pragma once

#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <random>

// The 'Game' struct holds all of the game-relevant state,
// and is called by the main loop.

struct Game {
	//Game creates OpenGL resources (i.e. vertex buffer objects) in its
	//constructor and frees them in its destructor.
	Game();
	~Game();

	//handle_event is called when new mouse or keyboard events are received:
	// (note that this might be many times per frame or never)
	//The function should return 'true' if it handled the event.
	bool handle_event(SDL_Event const &evt, glm::uvec2 window_size);

	void check_targets();
	void add_target();
	void check_enemies();
	void enemies_collision(uint32_t i);

	//update is called at the start of a new frame, after events are handled
	void update(float elapsed);

	//draw is called after update:
	void draw(glm::uvec2 drawable_size);
	void draw_score();

	//------- opengl resources -------

	//shader program that draws lit objects with vertex colors:
	struct {
		GLuint program = -1U; //program object

		//uniform locations:
		GLuint object_to_clip_mat4 = -1U;
		GLuint object_to_light_mat4x3 = -1U;
		GLuint normal_to_light_mat3 = -1U;
		GLuint sun_direction_vec3 = -1U;
		GLuint sun_color_vec3 = -1U;
		GLuint sky_direction_vec3 = -1U;
		GLuint sky_color_vec3 = -1U;

		//attribute locations:
		GLuint Position_vec4 = -1U;
		GLuint Normal_vec3 = -1U;
		GLuint Color_vec4 = -1U;
	} simple_shading;

	//mesh data, stored in a vertex buffer:
	GLuint meshes_vbo = -1U; //vertex buffer holding mesh data

	//The location of each mesh in the meshes vertex buffer:
	struct Mesh {
		GLint first = 0;
		GLsizei count = 0;
	};

	Mesh tile_mesh;
	Mesh cursor_mesh;
	Mesh cursor_mesh_red;
	Mesh duck_mesh;
	Mesh target_mesh;
	Mesh enemy_mesh;
	Mesh bg_mesh;
	std::vector<Mesh> numbers;
	
	GLuint meshes_for_simple_shading_vao = -1U; //vertex array object that describes how to connect the meshes_vbo to the simple_shading_program

	//------- game state -------
	float const max_power = 4.0f;
	float const min_r = 0.3f;

	glm::uvec2 board_size = glm::uvec2(5,4);
	std::vector< glm::mat4 > board_translations; //enemy movements
	std::vector< glm::mat4 > targets;
       	std::vector< float > bump; 
		//enemies go opposite way for a bit after bumping one another
	glm::quat cursor_rotation;
	glm::mat4 duck_pos;

	float power = 0.0f; //should only be between 0 and 1
	bool increase = true;
	int32_t cursor = 0; //should only be between -90 and 90
	float speed = 0.5f; //enemy speed
	uint32_t score = 0;

	float height = 0.0f; //ducks height
	float xpos = 0.0f; //ducks horizontal position 
	glm::vec2 velocity = glm::vec2(0.0f, 0.0f);

	struct {
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
		bool jump = false;
	} controls;

};

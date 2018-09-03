#include "Game.hpp"

#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>

using std::cout;
using std::endl;
//helper defined later; throws if shader compilation fails:
static GLuint compile_shader(GLenum type, std::string const &source);

Game::Game() {
	{ //create an opengl program to perform sun/sky (well, directional+hemispherical) lighting:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
				"#version 330\n"
				"uniform mat4 object_to_clip;\n"
				"uniform mat4x3 object_to_light;\n"
				"uniform mat3 normal_to_light;\n"
				"layout(location=0) in vec4 Position;\n" //note: layout keyword used to make sure that the location-0 attribute is always bound to something
				"in vec3 Normal;\n"
				"in vec4 Color;\n"
				"out vec3 position;\n"
				"out vec3 normal;\n"
				"out vec4 color;\n"
				"void main() {\n"
				"	gl_Position = object_to_clip * Position;\n"
				"	position = object_to_light * Position;\n"
				"	normal = normal_to_light * Normal;\n"
				"	color = Color;\n"
				"}\n"
				);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
				"#version 330\n"
				"uniform vec3 sun_direction;\n"
				"uniform vec3 sun_color;\n"
				"uniform vec3 sky_direction;\n"
				"uniform vec3 sky_color;\n"
				"in vec3 position;\n"
				"in vec3 normal;\n"
				"in vec4 color;\n"
				"out vec4 fragColor;\n"
				"void main() {\n"
				"	vec3 total_light = vec3(0.0, 0.0, 0.0);\n"
				"	vec3 n = normalize(normal);\n"
				"	{ //sky (hemisphere) light:\n"
				"		vec3 l = sky_direction;\n"
				"		float nl = 0.5 + 0.5 * dot(n,l);\n"
				"		total_light += nl * sky_color;\n"
				"	}\n"
				"	{ //sun (directional) light:\n"
				"		vec3 l = sun_direction;\n"
				"		float nl = max(0.0, dot(n,l));\n"
				"		total_light += nl * sun_color;\n"
				"	}\n"
				"	fragColor = vec4(color.rgb * total_light, color.a);\n"
				"}\n"
				);

		simple_shading.program = glCreateProgram();
		glAttachShader(simple_shading.program, vertex_shader);
		glAttachShader(simple_shading.program, fragment_shader);
		//shaders are reference counted so this makes sure they are freed after program is deleted:
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		//link the shader program and throw errors if linking fails:
		glLinkProgram(simple_shading.program);
		GLint link_status = GL_FALSE;
		glGetProgramiv(simple_shading.program, GL_LINK_STATUS, &link_status);
		if (link_status != GL_TRUE) {
			std::cerr << "Failed to link shader program." << std::endl;
			GLint info_log_length = 0;
			glGetProgramiv(simple_shading.program, GL_INFO_LOG_LENGTH, &info_log_length);
			std::vector< GLchar > info_log(info_log_length, 0);
			GLsizei length = 0;
			glGetProgramInfoLog(simple_shading.program, GLsizei(info_log.size()), &length, &info_log[0]);
			std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
			throw std::runtime_error("failed to link program");
		}
	}

	{ //read back uniform and attribute locations from the shader program:
		simple_shading.object_to_clip_mat4 = glGetUniformLocation(simple_shading.program, "object_to_clip");
		simple_shading.object_to_light_mat4x3 = glGetUniformLocation(simple_shading.program, "object_to_light");
		simple_shading.normal_to_light_mat3 = glGetUniformLocation(simple_shading.program, "normal_to_light");

		simple_shading.sun_direction_vec3 = glGetUniformLocation(simple_shading.program, "sun_direction");
		simple_shading.sun_color_vec3 = glGetUniformLocation(simple_shading.program, "sun_color");
		simple_shading.sky_direction_vec3 = glGetUniformLocation(simple_shading.program, "sky_direction");
		simple_shading.sky_color_vec3 = glGetUniformLocation(simple_shading.program, "sky_color");

		simple_shading.Position_vec4 = glGetAttribLocation(simple_shading.program, "Position");
		simple_shading.Normal_vec3 = glGetAttribLocation(simple_shading.program, "Normal");
		simple_shading.Color_vec4 = glGetAttribLocation(simple_shading.program, "Color");
	}

	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::u8vec4 Color;
	};
	static_assert(sizeof(Vertex) == 28, "Vertex should be packed.");

	{ //load mesh data from a binary blob:
		std::ifstream blob(data_path("meshes.blob"), std::ios::binary);
		//The blob will be made up of three chunks:
		// the first chunk will be vertex data (interleaved position/normal/color)
		// the second chunk will be characters
		// the third chunk will be an index, mapping a name (range of characters) to a mesh (range of vertex data)

		//read vertex data:
		std::vector< Vertex > vertices;
		read_chunk(blob, "dat0", &vertices);

		//read character data (for names):
		std::vector< char > names;
		read_chunk(blob, "str0", &names);

		//read index:
		struct IndexEntry {
			uint32_t name_begin;
			uint32_t name_end;
			uint32_t vertex_begin;
			uint32_t vertex_end;
		};
		static_assert(sizeof(IndexEntry) == 16, "IndexEntry should be packed.");

		std::vector< IndexEntry > index_entries;
		read_chunk(blob, "idx0", &index_entries);

		if (blob.peek() != EOF) {
			std::cerr << "WARNING: trailing data in meshes file." << std::endl;
		}

		//upload vertex data to the graphics card:
		glGenBuffers(1, &meshes_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, meshes_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//create map to store index entries:
		std::map< std::string, Mesh > index;
		for (IndexEntry const &e : index_entries) {
			if (e.name_begin > e.name_end || e.name_end > names.size()) {
				throw std::runtime_error("invalid name indices in index.");
			}
			if (e.vertex_begin > e.vertex_end || e.vertex_end > vertices.size()) {
				throw std::runtime_error("invalid vertex indices in index.");
			}
			Mesh mesh;
			mesh.first = e.vertex_begin;
			mesh.count = e.vertex_end - e.vertex_begin;
			auto ret = index.insert(std::make_pair(
						std::string(names.begin() + e.name_begin, names.begin() + e.name_end),
						mesh));
			if (!ret.second) {
				throw std::runtime_error("duplicate name in index.");
			}
		}

		//look up into index map to extract meshes:
		auto lookup = [&index](std::string const &name) -> Mesh {
			auto f = index.find(name);
			if (f == index.end()) {
				throw std::runtime_error("Mesh named '" + name + "' does not appear in index.");
			}
			return f->second;
		};
		cursor_mesh = lookup("White");
		cursor_mesh_red = lookup("Red");
		duck_mesh = lookup("Doll");
		target_mesh = lookup("Egg");
		enemy_mesh = lookup("Cube");
		
		//number meshes are from 
		//https://www.turbosquid.com/3d-models/free-numbers-1-2-3d-model/266953
		Mesh mesh0 = lookup("0");
		numbers.emplace_back(mesh0);	
		Mesh mesh1 = lookup("1");
		numbers.emplace_back(mesh1);	
		Mesh mesh2 = lookup("2");
		numbers.emplace_back(mesh2);	
		Mesh mesh3 = lookup("3");
		numbers.emplace_back(mesh3);	
		Mesh mesh4 = lookup("4");
		numbers.emplace_back(mesh4);	
		Mesh mesh5 = lookup("5");
		numbers.emplace_back(mesh5);	
		Mesh mesh6 = lookup("6");
		numbers.emplace_back(mesh6);	
		Mesh mesh7 = lookup("7");
		numbers.emplace_back(mesh7);	
		Mesh mesh8 = lookup("8");
		numbers.emplace_back(mesh8);	
		Mesh mesh9 = lookup("9");
		numbers.emplace_back(mesh9);	
	
	}

	{ //create vertex array object to hold the map from the mesh vertex buffer to shader program attributes:
		glGenVertexArrays(1, &meshes_for_simple_shading_vao);
		glBindVertexArray(meshes_for_simple_shading_vao);
		glBindBuffer(GL_ARRAY_BUFFER, meshes_vbo);
		//note that I'm specifying a 3-vector for a 4-vector attribute here, and this is okay to do:
		glVertexAttribPointer(simple_shading.Position_vec4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Position));
		glEnableVertexAttribArray(simple_shading.Position_vec4);
		if (simple_shading.Normal_vec3 != -1U) {
			glVertexAttribPointer(simple_shading.Normal_vec3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Normal));
			glEnableVertexAttribArray(simple_shading.Normal_vec3);
		}
		if (simple_shading.Color_vec4 != -1U) {
			glVertexAttribPointer(simple_shading.Color_vec4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Color));
			glEnableVertexAttribArray(simple_shading.Color_vec4);
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	GL_ERRORS();

	//----------------
	//set up game board with meshes and rolls:
	board_translations.reserve(board_size.x * board_size.y); 
	duck_pos = glm::mat4(
			0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f);
	cursor_rotation = glm::quat();
;
	//TODO change all the allocations to max number of pieces instead of board size?

	std::vector< Mesh const * > meshes{ &duck_mesh, &target_mesh, &enemy_mesh };

	//TODO add more enemy
	board_translations.emplace_back(glm::mat4(
				0.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 3.0f, 0.0f, 0.0f));

	
	std::mt19937 mt(0xbead1234); //wtf apparently random num gen
	for(uint32_t i = 0; i<7; i++){
		add_target();
	}
}

Game::~Game() {
	glDeleteVertexArrays(1, &meshes_for_simple_shading_vao);
	meshes_for_simple_shading_vao = -1U;

	glDeleteBuffers(1, &meshes_vbo);
	meshes_vbo = -1U;

	glDeleteProgram(simple_shading.program);
	simple_shading.program = -1U;

	GL_ERRORS();
}

bool Game::handle_event(SDL_Event const &evt, glm::uvec2 window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	//move duck jump angle and power
	if (evt.type == SDL_KEYDOWN|| evt.type == SDL_KEYUP) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_LEFT) {
			controls.left = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
			controls.right = (evt.type == SDL_KEYDOWN);
			return true;
		}
		if (evt.key.keysym.scancode == SDL_SCANCODE_SPACE) {
			controls.up = (evt.type == SDL_KEYDOWN);
			if(controls.up == false) {
				controls.jump = true;
				velocity = glm::vec2(cursor/30.0f, 2.5*power);
			}
			return true;
		}
	}
	return false;
}

void Game::add_target(){
	//following chunk is from the cppreference on random device
	//which I looked up after a recommendation from Thejaswi Kadur
	//https://en.cppreference.com/w/cpp/numeric/random/random_device
	std::random_device rd;
    	std::uniform_int_distribution<int> dist(0, 0xbead1234);
	std::mt19937 mt(dist(rd));
	
	float newX = mt()%100/20.0f;
	float newY = mt()%100/28.0f;
	while(newY<1.0f) newY = mt()%100/26.0f;
	targets.emplace_back(glm::mat4(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			newX, newY, 0.0f, 1.0f));
}

void Game::check_targets(){
	for(uint32_t i = 0; i < 7; i++){
		glm::vec2 t_pos = glm::vec2(targets[i][3][0], 
						targets[i][3][1]);
		glm::vec2 c_pos = glm::vec2(duck_pos[3][0],
						height);
		
		float distance = std::sqrt(std::pow((c_pos[0]-t_pos[0]), 2.0f)
				+std::pow((c_pos[1]-t_pos[1]), 2.0f));
		if(distance <= min_r){
			targets.erase(targets.begin()+i);
			add_target();
			score++;
			//new enemy spawned for each 10 points gained
			
			if(score%10==0){
				board_translations.emplace_back(glm::mat4(
					0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 3.0f, 0.0f, 0.0f));
			}

		}
	}
}

void Game::check_enemies(){
	for(uint32_t i = 0; i < board_translations.size(); i++){
		glm::vec2 t_pos = glm::vec2(board_translations[i][3][0], 
					board_translations[i][3][1]);
		glm::vec2 c_pos = glm::vec2(duck_pos[3][0],
						height);
		float distance = std::sqrt(std::pow((c_pos[0]-t_pos[0]), 2.0f)
				+std::pow((c_pos[1]-t_pos[1]), 2.0f));
		if(distance <= min_r){
			//TODO gameover
		}
	}
}

void Game::update(float elapsed) {
	//if the roll keys are pressed, rotate everything on the same row or column as the cursor:
	glm::quat dr = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	float amt = elapsed * 1.0f;
	int32_t angle = 1;
	if (controls.left && cursor>-90) {
		cursor-=angle;
		dr = glm::angleAxis(amt, glm::vec3(0.0f, 0.0f, 1.0f)) * dr;
	}else if (controls.right && cursor<90) {
		cursor+=angle;
		dr = glm::angleAxis(-amt, glm::vec3(0.0f, 0.0f, 1.0f)) * dr;
	}else if (controls.up){
		if(increase && power<max_power)
			power+=0.1f;
		else if(!increase && power>0.0f)
			power-=0.1f;

		if(increase && power>=max_power) increase = false;
		if(!increase && power<=0) increase = true;
	}

	if (dr != glm::quat()) {
		glm::quat &r = cursor_rotation;
		r = glm::normalize(dr * r);
	}

	if(controls.jump){
		//referenced the discussion here
		//https://gamedev.stackexchange.com/questions/15708/how-can-i-implement-gravity
		//although i guess i did take ap physics c...
		height += elapsed*(velocity.y+elapsed*-4.9f);
		xpos += elapsed*velocity.x;
		velocity.y += elapsed*-4.9;
	
		if(xpos < -0.5f || xpos > 5.5f){
			velocity.x *= -0.8f;
		}

		if(height > 3.6){
			velocity.y = -2.0f;
		}

		if(height<0.01f){
			height = 0.0f;
			power = 0;
			velocity.x = 0.0f;
			controls.jump = false;
		}
		duck_pos = glm::mat4(
			0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f,
			xpos, height, 0.0f, 0.0f);
		check_targets();
	}

	for(uint32_t i = 0; i < board_translations.size(); i++){
		glm::vec2 target = glm::vec2(duck_pos[3][0], duck_pos[3][1]);
		glm::vec2 current = glm::vec2(board_translations[i][3][0],
					board_translations[i][3][1]);
		board_translations[i][3][0] += (target[0]-current[0])
						/(400.0f*speed);
		board_translations[i][3][1] += (height-current[1])
						/(400.0f*speed);
	}
	check_enemies();

}

void Game::draw(glm::uvec2 drawable_size) {
	//Set up a transformation matrix to fit the board in the window:
	glm::mat4 world_to_clip;
	{
		float aspect = float(drawable_size.x) / float(drawable_size.y);

		//want scale such that board * scale fits in [-aspect,aspect]x[-1.0,1.0] screen box:
		float scale = glm::min(
				2.0f * aspect / float(board_size.x),
				2.0f / float(board_size.y)
				);

		//center of board will be placed at center of screen:
		glm::vec2 center = 0.5f * glm::vec2(board_size);

		//NOTE: glm matrices are specified in column-major order
		world_to_clip = glm::mat4(
				scale / aspect, 0.0f, 0.0f, 0.0f,
				0.0f, scale, 0.0f, 0.0f,
				0.0f, 0.0f,-1.0f, 0.0f,
				-(scale / aspect) * center.x, -scale * center.y, 0.0f, 1.0f
				);
	}

	//set up graphics pipeline to use data from the meshes and the simple shading program:
	glBindVertexArray(meshes_for_simple_shading_vao);
	glUseProgram(simple_shading.program);

	glUniform3fv(simple_shading.sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(simple_shading.sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(simple_shading.sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
	glUniform3fv(simple_shading.sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));

	//helper function to draw a given mesh with a given transformation:
	auto draw_mesh = [&](Mesh const &mesh, glm::mat4 const &object_to_world) {
		//set up the matrix uniforms:
		if (simple_shading.object_to_clip_mat4 != -1U) {
			glm::mat4 object_to_clip = world_to_clip * object_to_world;
			glUniformMatrix4fv(simple_shading.object_to_clip_mat4, 1, GL_FALSE, glm::value_ptr(object_to_clip));
		}
		if (simple_shading.object_to_light_mat4x3 != -1U) {
			glUniformMatrix4x3fv(simple_shading.object_to_light_mat4x3, 1, GL_FALSE, glm::value_ptr(object_to_world));
		}
		if (simple_shading.normal_to_light_mat3 != -1U) {
			//NOTE: if there isn't any non-uniform scaling in the object_to_world matrix, then the inverse transpose is the matrix itself, and computing it wastes some CPU time:
			glm::mat3 normal_to_world = glm::inverse(glm::transpose(glm::mat3(object_to_world)));
			glUniformMatrix3fv(simple_shading.normal_to_light_mat3, 1, GL_FALSE, glm::value_ptr(normal_to_world));
		}

		//draw the mesh:
		glDrawArrays(GL_TRIANGLES, mesh.first, mesh.count);
	};
	
	if(controls.up || controls.right || controls.left){
		draw_mesh(cursor_mesh, //white jump bar
			glm::mat4(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 1.0f
				)*glm::mat4_cast(cursor_rotation) //jump angle
				+duck_pos);
	
		draw_mesh(cursor_mesh_red, glm::mat4( //red jump bar
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 1.0f
			)*glm::mat4_cast(cursor_rotation) //jump angle
			*glm::mat4(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f+0.6f*power, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f) +duck_pos); //jump power
	}

	//draw all the targets
	for(uint32_t i = 0; i<targets.size(); i++){
		draw_mesh(target_mesh, targets[i]);
	}

	draw_mesh(duck_mesh, glm::mat4(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0, 0.5f, 0.0f, 1.0f)+ (duck_pos));

	for(uint32_t i = 0; i < board_translations.size(); i++){
		draw_mesh(enemy_mesh,
				glm::mat4(
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					0.5f, 0.5f, 0.0f, 1.0f
					) + board_translations[i]
				);
	}
	
	uint32_t remainder = score;
	float xcoord = 3.8f;
	do{
		int digit = remainder%10;
		draw_mesh(numbers[digit],
			glm::mat4(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				xcoord, 2.5f, 0.0f, 1.0f));
				
		remainder /= 10;
		xcoord -= 0.1f;
	}while(remainder>0);
	
	glUseProgram(0);

	GL_ERRORS();
}



//create and return an OpenGL vertex shader from source:
static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = GLint(source.size());
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, GLsizei(info_log.size()), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

#include <algorithm>
#include <string>
#include <cmath>
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <random>
#include <unordered_map>
#include <math.h>

#include <glad/glad.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>

#include "gl_utils.hpp"
#include <tgaimage.h>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

enum class HexType
{
	Empty = 0,
	Player,
	Wall
};


float rad_for_hex(int i) {
	float angle_deg = 60 * i + 30;
	return M_PI / 180 * angle_deg;
}

void hex_at(ShaderProgram& program, float x, float y, float r, color c) {
	VBO vbo{program};

	vbo.push_vertex(x, y, c);

	int rot = 0; // 1;
	for (int i = rot; i < 7 + rot; i++) {
		float ri = rad_for_hex(i);
		c = c.mut(0.03f);
		vbo.push_vertex(x + r * cos(ri), y + r * sin(ri), c);
	}

	vbo.draw(GL_TRIANGLE_FAN);
}

class mat
{
	std::size_t mat_dim_;
	matrix<HexType> data_;
	matrix<std::pair<float, float>> positions_;

public:
	int m;
	int player_row = 0, player_col = 0;
	bool player_set = false;

	mat(int m) : mat_dim_(2 * m + 1),
	             data_(mat_dim_, mat_dim_),
	             positions_(mat_dim_, mat_dim_),
	             m(m) {

		// TODO - ugly
		std::fill(positions_.vs.begin(), positions_.vs.end(), std::make_pair<float, float>(INFINITY, INFINITY));
	}

	mat(const mat&) = delete;

	HexType& operator()(int col, int row) {
		return data_(col, row);
	}

	std::pair<float, float>& pos(int col, int row) {
		return positions_(col, row);
	}

	bool move_player(int col, int row) {
		if (fmax(std::abs(row), std::abs(col)) >= m || fmin(row, col) < 0)
			return false;
		if ((*this)(col, row) == HexType::Wall)
			return false;

		if (player_set) {
			(*this)(player_col, player_row) = HexType::Empty;
		}

		player_row = row;
		player_col = col;

		(*this)(col, row) = HexType::Player;

		player_set = true;
		return true;
	}

	bool step_player(int dcol, int drow) {
		if (player_set) {
			return move_player(player_col + dcol, player_row + drow);
		}
		return false;
	}

	void highlight_near(float rel_x, float rel_y) {
		int closest_x = 0;
		int closest_y = 0;
		float min = INFINITY;
		for (size_t i = 0; i < positions_.m; i++) {
			for (size_t j = 0; j < positions_.n; j++) {
				auto pos = positions_(i, j);
				float d1 = pos.first - rel_x;
				float d2 = pos.second - rel_y;
			
				if (d1 * d1 + d2 * d2 < min) {
					closest_x = j;
					closest_y = i;
				}
			}
		}

		(*this)(closest_y - positions_.m, closest_x - positions_.m) = HexType::Player;
	}
};


void handlePlayerStep(Sint32 sym, mat& grid) {
	switch (sym) {
		case 'a':
			grid.step_player(-1, 0);
			break;

		case 'd':
			grid.step_player(1, 0);
			break;

		case 'z':
			grid.step_player(0, -1);
			break;

		case 'e':
			grid.step_player(0, 1);
			break;

		case 'c':
			grid.step_player(1, -1);
			break;

		case 'q':
			grid.step_player(-1, 1);
			break;
	}
}

color color_for_type(HexType type) {
	switch (type) {
		case HexType::Empty:
			return {0.4f, 0.2f, 0.4f};
		case HexType::Wall:
			return {0.1f, 0.03f, 0.1f};
		case HexType::Player:
			return {0.7f, 0.4f, 0.7f};
		default:
			throw "invalid hex type";
	}

}

void game_loop(SDL_Window* window) {
	std::vector<float> vertices;

	// TODO - proc tohle nefunguje?
	// glEnable(GL_POLYGON_SMOOTH | GL_MULTISAMPLE);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	ShaderProgram program{"vertex.glsl", "fragment.glsl"};
	std::cerr << glGetError() << std::endl;

	mat grid{10};

	grid.move_player(0, 0);
	grid(0, 1) = HexType::Wall;
	grid(0, 2) = HexType::Wall;

	SDL_Event windowEvent;
	while (true) {
		if (SDL_PollEvent(&windowEvent)) {
			if (windowEvent.type == SDL_MOUSEMOTION) {
				std::cout << windowEvent.motion.x << " " << windowEvent.motion.y << std::endl;
				float rel_x = static_cast<float>(windowEvent.motion.x) / SCREEN_WIDTH;
				float rel_y = static_cast<float>(windowEvent.motion.y) / SCREEN_HEIGHT;

				grid.highlight_near(rel_x, rel_y);
			}

			if (windowEvent.type == SDL_QUIT ||
				(windowEvent.type == SDL_KEYUP &&
					windowEvent.key.keysym.sym == SDLK_ESCAPE))
				break;

			if (windowEvent.type == SDL_KEYDOWN) {
				handlePlayerStep(windowEvent.key.keysym.sym, grid);
			}
		}

		glClearColor(0.3f, 0.2f, 0.3f, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		float start_x = -0.5f;
		float start_y = -0.5f;

		float radius = 0.1f;
		float width = cos(30 * M_PI / 180) * radius * 2;
		float height_offset = radius + sin(30 * M_PI / 180) * radius;

		for (int row = 0; row < grid.m; ++row) {
			for (int col = 0; col < grid.m; ++col) {

				float draw_x = start_x;
				float draw_y = start_y;

				// axial q-change
				draw_x += col * width;
				// axial r-change
				draw_x += row * (width / 2);
				draw_y += row * height_offset;

				grid.pos(col, row) = {draw_x, draw_y};
				//std::cout << draw_x << " " << draw_y << std::endl;

				color c = color_for_type(grid(col, row));

				hex_at(program, draw_x, draw_y, radius, c);
				c = c.mut(0.004f);
			}
		}

		SDL_GL_SwapWindow(window);
	}
}

int main(int argc, char** argv) {
	// TODO - error handling
	SDL_Init(SDL_INIT_VIDEO);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	// TODO - error handling
	SDL_Window* window = SDL_CreateWindow(
		"OpenGL", 300, 300, // TODO - better default screen position
		SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);

	// TODO - error handling
	SDL_GLContext context = SDL_GL_CreateContext(window);

	gladLoadGLLoader(SDL_GL_GetProcAddress);

	game_loop(window);

	SDL_GL_DeleteContext(context);
	// SDL_Delay(1000);
	SDL_Quit();

	return 0;
}


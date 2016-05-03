// Some of the includes include windef.h, which in turn
// defines min/max macros in the global namespace, which will clash
// with the std::min/max functions.
#define NOMINMAX

#include <algorithm>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_sdl.h>

#include <format.h>

#include <stopwatch.hpp>
#include <gl_utils.hpp>
#include <model.hpp>
#include <simulation.hpp>
#include <input_manager.hpp>
#include <lodepng.h>

#include <glm/glm.hpp>

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"

unsigned char ttf_buffer[1 << 20];
unsigned char temp_bitmap[512 * 512];

stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs
GLuint ftex;

void my_stbtt_initfont(void)
{
	fread(ttf_buffer, 1, 1 << 20, fopen("c:/windows/fonts/times.ttf", "rb"));
	stbtt_BakeFontBitmap(ttf_buffer, 0, 32.0, temp_bitmap, 512, 512, 32, 96, cdata); // no guarantee this fits!
	// can free ttf_buffer at this point
	glGenTextures(1, &ftex);
	glBindTexture(GL_TEXTURE_2D, ftex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 512, 512, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
	// can free temp_bitmap at this point
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void my_stbtt_print(float x, float y, char* text)
{
	gl::Batch b;
	// assume orthographic projection with units = screen pixels, origin at top left
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, ftex);

	while (*text++) {
		if (*text >= 32 && *text < 128) {
			stbtt_aligned_quad q;
			stbtt_GetBakedQuad(cdata, 512, 512, *text - 32, &x, &y, &q, 1);

			//fmt::printf("%f,%f\t%f,%f\n", q.x0, q.y0, q.s0, q.t1);
			float z = -0.5f;

			b.push_back({{q.x0, q.y0, z}, {q.s0, q.t1}});
			b.push_back({{q.x1, q.y0, z}, {q.s1, q.t1}});
			b.push_back({{q.x1, q.y1, z}, {q.s1, q.t0}});
			b.push_back({{q.x0, q.y1, z}, {q.s0, q.t0}});
		}
	}

	b.draw_arrays();
	//glBegin(GL_QUADS);
	//while (*text) {
	//	if (*text >= 32 && *text < 128) {
	//		stbtt_aligned_quad q;
	//		stbtt_GetBakedQuad(cdata, 512, 512, *text - 32, &x, &y, &q, 1);//1=opengl & d3d10+,0=d3d9
	//		glTexCoord2f(q.s0, q.t1); glVertex2f(q.x0, q.y0);
	//		glTexCoord2f(q.s1, q.t1); glVertex2f(q.x1, q.y0);
	//		glTexCoord2f(q.s1, q.t0); glVertex2f(q.x1, q.y1);
	//		glTexCoord2f(q.s0, q.t0); glVertex2f(q.x0, q.y1);
	//	}
	//	++text;
	//}
	//glEnd();
}

using namespace model;
using namespace glm;

namespace game
{
	void paint_at(Position pos, float radius, Color color)
	{
		gl::Batch b;
		b.push_hex(pos, color, radius);
		b.draw_arrays();
	}

	Coord hex_at_mouse(const mat4& proj, Arena& arena, int x, int y)
	{
		auto rel_mouse = mouse2gl(x, y);
		auto view_mouse = inverse(proj) * vec4(rel_mouse.x, rel_mouse.y, 0.0f, 1.0f);
		return arena.hex_near({view_mouse.x, view_mouse.y});
	}

	void game_loop(SDL_Window* window)
	{
		using namespace model;
		using namespace glm;

		ImGui_ImplSdlGL3_Init(window);

		Stopwatch st;

		GameInstance game(30);
		Arena& arena = game.arena;
		PlayerInfo& info = game.info;

		st.start();
		arena.regenerate_geometry();
		st.print("Arena vertices");

		Mob& player = info.add_mob(generator::random_mob());
		player.c = {0, 0};

		GLuint vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		GLuint vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		gl::Vertex::setup_attributes();

		gl::Shader shader{"vertex.glsl", "fragment.glsl"};
		shader.use();

		gl::Camera camera;

		SDL_GL_SetSwapInterval(1);
		my_stbtt_initfont();

		std::vector<unsigned char> tex_buf;
		unsigned w, h;
		lodepng::decode(tex_buf, w, h, "c:/dev/chicken.png");

		gl::Texture2D t;
		t.load_png("c:/dev/chicken.png");

		gl::Shader spriteShader("res/sprite.vs.glsl", "res/sprite.fs.glsl");
		gl::SpriteRenderer sprites{ spriteShader };


		InputManager input_manager;
		while (true) {
			glClearColor(0.3f, 0.2f, 0.3f, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			ImGui_ImplSdlGL3_NewFrame(window);

			bool keep_running = input_manager.handle_events(camera, arena, player);
			if (!keep_running) {
				break;
			}

			camera.update_and_load_camera();
			//arena.draw_vertices();


			sprites.draw_sprite(t, { 0, 0 });
			//gl::Batch b1;
			//b1.push_back(gl::Vertex({ 0, 0, 0 }, { 1,1,1,1 }, { 0, 0 }, 1));
			//b1.push_back(gl::Vertex({ 1, 0, 0 }, { 1,1,1,1 }, { 1, 0 }, 1));
			//b1.push_back(gl::Vertex({ 0, 1, 0 }, { 1,1,1,1 }, { 0, 1 }, 1));

			//b1.draw_arrays();
			//b.push_triangle({0, 0}, {0, 1}, {1, 0}, 0, gl::ColorTex({}))

			auto highlight_pos = arena.pos(input_manager.highlight_hex);
			paint_at(highlight_pos, Arena::radius, color_for_type(HexType::Player));

			Color highlight_color{0.85f, 0.75f, 0.85f};
			auto mouse_pos = arena.pos(input_manager.mouse_hex);
			paint_at(mouse_pos, Arena::radius, highlight_color);

			for (Coord c : input_manager.highlight_path) {
				paint_at(arena.pos(c), Arena::radius, highlight_color);
			}

			gl::Batch b;

			for (auto& mob : info.mobs) {
				auto pos = arena.pos(mob.c);
				paint_at(pos, Arena::radius, color_for_type(HexType::Player));

				Healthbar::draw(pos, b, 0.7f, 0.5f);
			}

			b.draw_arrays();

			my_stbtt_print(0, 0, "hello");

			ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiSetCond_FirstUseEver);
			ImGui::Begin("Framerate");
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();

			ImGui::Begin("Profiling");
			if (ImGui::Button("Dummy profile")) {
				simulation::dummy_profiling();
			}

			if (simulation::profiling_results.size() > 0) {
				for (auto& res : simulation::profiling_results) {
					ImGui::Text(res.c_str());
				}
			}

			ImGui::End();
			ImGui::Render();

			SDL_GL_SwapWindow(window);
		}
	}
}

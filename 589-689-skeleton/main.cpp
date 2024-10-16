#include <iostream>

// Window.h `#include`s ImGui, GLFW, and glad in correct order.
#include "Window.h"

#include "Geometry.h"
#include "GLDebug.h"
#include "Log.h"
#include "ShaderProgram.h"
#include "Shader.h"

// CALLBACKS
class MyCallbacks : public CallbackInterface {

public:
	// Constructor. We use values of -1 for attributes that, at the start of
	// the program, have no meaningful/"true" value.
	MyCallbacks(ShaderProgram& shader, int screenWidth, int screenHeight)
		: shader(shader)
		, currentFrame(0)
		, leftMouseActiveVal(false)
		, lastLeftPressedFrame(-1)
		, lastRightPressedFrame(-1)
		, screenMouseX(-1.0)
		, screenMouseY(-1.0)
		, screenWidth(screenWidth)
		, screenHeight(screenHeight)
	{}

	virtual void keyCallback(int key, int scancode, int action, int mods) {
		if (key == GLFW_KEY_R && action == GLFW_PRESS) {
			shader.recompile();
		}
	}

	virtual void mouseButtonCallback(int button, int action, int mods) {
		// If we click the mouse on the ImGui window, we don't want to log that
		// here. But if we RELEASE the mouse over the window, we do want to
		// know that!
		auto& io = ImGui::GetIO();
		if (io.WantCaptureMouse && action == GLFW_PRESS) return;


		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			leftMouseActiveVal = true;
			lastLeftPressedFrame = currentFrame;
		}
		if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
			lastRightPressedFrame = currentFrame;
		}

		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
			leftMouseActiveVal = false;
		}
	}

	// Updates the screen width and height, in screen coordinates
	// (not necessarily the same as pixels)
	virtual void windowSizeCallback(int width, int height) {
		screenWidth = width;
		screenHeight = height;
	}

	// Sets the new cursor position, in screen coordinates
	virtual void cursorPosCallback(double xpos, double ypos) {
		screenMouseX = xpos;
		screenMouseY = ypos;
	}

	// Whether the left mouse was pressed down this frame.
	bool leftMouseJustPressed() {
		return lastLeftPressedFrame == currentFrame;
	}

	// Whether the left mouse button is being pressed down at all.
	bool leftMouseActive() {
		return leftMouseActiveVal;
	}

	// Whether the right mouse button was pressed down this frame.
	bool rightMouseJustPressed() {
		return lastRightPressedFrame == currentFrame;
	}

	// Tell the callbacks object a new frame has begun.
	void incrementFrameCount() {
		currentFrame++;
	}

	// Converts the cursor position from screen coordinates to GL coordinates
	// and returns the result.
	glm::vec2 getCursorPosGL() {
		glm::vec2 screenPos(screenMouseX, screenMouseY);
		// Interpret click as at centre of pixel.
		glm::vec2 centredPos = screenPos + glm::vec2(0.5f, 0.5f);
		// Scale cursor position to [0, 1] range.
		glm::vec2 scaledToZeroOne = centredPos / glm::vec2(screenWidth, screenHeight);

		glm::vec2 flippedY = glm::vec2(scaledToZeroOne.x, 1.0f - scaledToZeroOne.y);

		// Go from [0, 1] range to [-1, 1] range.
		return 2.f * flippedY - glm::vec2(1.f, 1.f);
	}
	
	// Takes in a list of points, given in GL's coordinate system,
	// and a threshold (in screen coordinates) 
	// and then returns the index of the first point within that distance from
	// the cursor.
	// Returns -1 if no such point is found.
	int indexOfPointAtCursorPos(const std::vector<glm::vec3>& glCoordsOfPointsToSearch, float screenCoordThreshold) {
		// First, we conver thte points from GL to screen coordinates.
		std::vector<glm::vec3> screenCoordVerts;
		for (const auto& v : glCoordsOfPointsToSearch) {
			screenCoordVerts.push_back(glm::vec3(glPosToScreenCoords(v), 0.f));
		}

		// We make sure we interpret the cursor position as at the centre of
		// the relevant pixel, for consistency with getCursorPosGL().
		glm::vec3 cursorPosScreen(screenMouseX + 0.5f, screenMouseY + 0.5f, 0.f);


		for (size_t i = 0; i < screenCoordVerts.size(); i++) {
			// Return i if length of difference vector within threshold.
			glm::vec3 diff = screenCoordVerts[i] - cursorPosScreen;
			if (glm::length(diff) < screenCoordThreshold) {
				return i;
			}
		}
		return -1; // No point within threshold found.
	}

private:
	int screenWidth;
	int screenHeight;

	double screenMouseX;
	double screenMouseY;

	int currentFrame;

	bool leftMouseActiveVal;

	int lastLeftPressedFrame;
	int lastRightPressedFrame;

	ShaderProgram& shader;

	// Converts GL coordinates to screen coordinates.
	glm::vec2 glPosToScreenCoords(glm::vec2 glPos) {

		// Convert the [-1, 1] range to [0, 1]
		glm::vec2 scaledZeroOne = 0.5f * (glPos + glm::vec2(1.f, 1.f));
		glm::vec2 flippedY = glm::vec2(scaledZeroOne.x, 1.0f - scaledZeroOne.y);
		glm::vec2 screenPos = flippedY * glm::vec2(screenWidth, screenHeight);
		return screenPos;
	}
};

// update method for delta based on u
int delta(std::vector<float> U, float u, int k, int m) {
	for (int i = 0; i < m + k - 1; i++) {
		if ((u >= U[i]) && (u < U[i + 1])) {
			return i;
		}
	}
	return -1;
}

// calculates and returns the standard knot sequence for a given k and m
std::vector<float> standardKnot(float k, float m) {

	std::vector<float> U;

	// generate standard knot sequence
	float spacing = 1 / (m - k + 2);
	for (int i = 0; i < (m + k + 1); i++) {
		if (i < k) {
			U.push_back(0.f);
		}
		else if (i >= k && i < (m + 1)) {
			U.push_back(U[i-1] + spacing);
			
		}
		else {
			U.push_back(1.f);
		}
	}

	return U;
}

// generates a b-spline curve based on the given parameters
CPU_Geometry efficientBSpline(std::vector<glm::vec3> E, std::vector<float> U, std::vector<int> M, int k, int m, float u_inc) {

	CPU_Geometry cpuGeom;

	for (float u = U[k - 1]; u < U[m + 1]; u += u_inc) {
		std::vector<glm::vec3> C;

		// update method
		int d = delta(U, u, k, m);
		int i;
		for (i = 0; i < (k - 1); i++){

			// nonzero coefficients
			C.push_back(E[d - i]);
		}

		for (int r = k; r > 2; r--) {
			i = d;
			for (int s = 0; s < (r - 2); s++) {
				float omega = (u - U[i]) / (U[i + r - 1] - U[i]);
				C[s] = (omega * C[s] + (1 - omega) * C[s + 1]); 
				i-=1;
			}
		}

		// Save the calculated point on the curve
		cpuGeom.verts.push_back(C[0]);
		cpuGeom.cols.push_back(glm::vec3{ 1.f,0.75f,0.2f });
	}
	return cpuGeom;
}

int main() {
	Log::debug("Starting main");

	// WINDOW
	glfwInit();
	Window window(800, 800, "CPSC 589/689"); // could set callbacks at construction if desired
	GLDebug::enable();

	// SHADERS
	ShaderProgram shader("shaders/test.vert", "shaders/test.frag");
	auto cb = std::make_shared<MyCallbacks>(shader, window.getWidth(), window.getHeight());

	// CALLBACKS
	window.setCallbacks(cb);
	window.setupImGui(); // Make sure this call comes AFTER GLFW callbacks set.

	// GEOMETRY
	CPU_Geometry cpuGeom, curve;
	GPU_Geometry gpuGeom, curveGPU;

	// Variables that ImGui will alter.
	int k = 2;
	float u_inc = 0.2f;
	bool drawPoints = true; // Whether to draw connecting lines
	bool drawCurve = true; // Whether to draw control points

	int selectedPointIndex = -1; // Used for point dragging & deletion

	// RENDER LOOP
	while (!window.shouldClose()) {

		// Tell callbacks object a new frame's begun BEFORE polling events!
		cb->incrementFrameCount();
		glfwPollEvents();

		// If mouse just went down, see if it was on a point.
		if (cb->leftMouseJustPressed() || cb->rightMouseJustPressed()) {
		
			float threshold = 6.f;
			selectedPointIndex = cb->indexOfPointAtCursorPos(cpuGeom.verts, threshold);
		}

		if (cb->leftMouseJustPressed()) {
			if (selectedPointIndex < 0) {

				// If we just clicked empty space, add new point.
				cpuGeom.verts.push_back(glm::vec3(cb->getCursorPosGL(), 0.f));
				cpuGeom.cols.push_back(glm::vec3(0.f,1.f,0.f));
				gpuGeom.setVerts(cpuGeom.verts);
				gpuGeom.setCols(cpuGeom.cols);
			}
		}
		else if (cb->rightMouseJustPressed()) {
			if (selectedPointIndex >= 0) {

				// If we right-clicked on a vertex, erase it.
				cpuGeom.verts.erase(cpuGeom.verts.begin() + selectedPointIndex);
				cpuGeom.cols.erase(cpuGeom.cols.begin() + selectedPointIndex);
				selectedPointIndex = -1; // So that we don't drag in next frame.
				gpuGeom.setVerts(cpuGeom.verts);
				gpuGeom.setCols(cpuGeom.cols);
			}
		}
		else if (cb->leftMouseActive() && selectedPointIndex >= 0) {

			// Drag selected point.
			cpuGeom.verts[selectedPointIndex] = glm::vec3(cb->getCursorPosGL(), 0.f);
			gpuGeom.setVerts(cpuGeom.verts);
		}

		bool change = false; // Whether any ImGui variable's changed.

		// Three functions that must be called each new frame.
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// ImGui stuff
		ImGui::Begin("Sample window.");
		ImGui::Text("Sample text.");
		change |= ImGui::SliderInt("k", &k, 2, 10);
		change |= ImGui::SliderFloat("u_increment", &u_inc, 0.001f, 1.f);
		change |= ImGui::Checkbox("Draw control pts", &drawPoints);
		change |= ImGui::Checkbox("Draw curve", &drawCurve);

		// Clear screen
		if (ImGui::Button("Clear")) {
			change = true;
			cpuGeom.verts.clear();
			cpuGeom.cols.clear();
			curve.verts.clear();
			curve.cols.clear();
			gpuGeom.setVerts(cpuGeom.verts);
			gpuGeom.setCols(cpuGeom.cols);
			curveGPU.setVerts(curve.verts);
			curveGPU.setCols(curve.cols);
		}

		//ImGui::Text("Average %.1f ms/frame (%.1f fps)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		// If there are at least two control points specified
		if (cpuGeom.verts.size() > 1) {

			// Calculate standard knot sequence based on given k and m (# of control points - 1)
			std::vector <float> U = standardKnot(k, cpuGeom.verts.size()-1);

			// Efficient b-spline algorithm
			curve = efficientBSpline(cpuGeom.verts, U, std::vector <int> {1}, k, cpuGeom.verts.size() - 1, u_inc);

			// Gpu geometry for the curve needs to be set
			curveGPU.setVerts(curve.verts);
			curveGPU.setCols(curve.cols);
		}

		// ImGui stuff
		ImGui::End();
		ImGui::Render();

		shader.use();
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_FRAMEBUFFER_SRGB);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		if (drawCurve) {
			curveGPU.bind();
			glDrawArrays(GL_LINE_STRIP, 0, GLsizei(curve.verts.size()));
		}
	
		if (drawPoints) {
			glPointSize(6.f);
			gpuGeom.bind();
			glDrawArrays(GL_POINTS, 0, GLsizei(cpuGeom.verts.size()));
		}

		glDisable(GL_FRAMEBUFFER_SRGB); // disable sRGB for things like imgui

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		window.swapBuffers();
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
	return 0;
}

// Variables for the commented-out widgets.
/*
int sampleInt = 0;
float sampleFloat = 0.f;
float sampleDragFloat = 0.f;
float sampleAngle = 0.f;
float sampleFloatPair[2] = { 1.f, 2.f };

*/

// Commented out ImGui widgets from tutorial video.
// Check ImGui samples/GitHub/etc. for more cool options, like graphs!
/*
change |= ImGui::InputInt("sample int", &sampleInt, 2);
change |= ImGui::InputFloat("sample float", &sampleFloat, 0.5f);
change |= ImGui::DragFloat("sample dragger:", &sampleDragFloat, 1.f, 0.f, 100.f);
bool changeAngle = false;
change |= changeAngle = ImGui::SliderAngle("sample angle", &sampleAngle);
change |= ImGui::SliderFloat2("sample float pair", (float*)&sampleFloatPair, -15, 15);
if (change) {
	std::cout << "Change detected! " << std::endl;
	if (changeAngle) std::cout << "Sample angle: " << sampleAngle << std::endl;
}
*/

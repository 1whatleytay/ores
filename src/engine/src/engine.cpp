#include <engine/engine.h>

#include <glad/glad.h>

#include <chrono>

#include <fmt/printf.h>

#include <array>
#include <fstream>
#include <sstream>

namespace {
    Handle loadShaderProgram() {
        auto getSource = [](const std::string &path) -> std::string {
            std::ifstream stream(path);
            if (!stream.is_open())
                throw std::exception();

            std::stringstream buffer;
            buffer << stream.rdbuf();

            return buffer.str();
        };

        std::string vertSource = getSource("assets/shaders/vert.glsl");
        std::string fragSource = getSource("assets/shaders/frag.glsl");

        const char *vertPtr = vertSource.c_str();
        const char *fragPtr = fragSource.c_str();

        Handle vertShader(glCreateShader(GL_VERTEX_SHADER), glDeleteShader);
        Handle fragShader(glCreateShader(GL_FRAGMENT_SHADER), glDeleteShader);

        glShaderSource(vertShader, 1, &vertPtr, nullptr);
        glShaderSource(fragShader, 1, &fragPtr, nullptr);

        auto verify = [](GLuint s) {
            GLint status;
            glGetShaderiv(s, GL_COMPILE_STATUS, &status);

            if (!status) {
                std::array<char, 1000> buffer = { };

                int32_t length;
                glGetShaderInfoLog(s, buffer.size(), &length, buffer.data());

                fmt::print("Compile Error: {}\n", std::string(buffer.data(), length));

                throw std::exception();
            }
        };

        glCompileShader(vertShader);
        glCompileShader(fragShader);

        verify(vertShader);
        verify(fragShader);

        Handle program(glCreateProgram(), glDeleteProgram);
        glAttachShader(program, vertShader);
        glAttachShader(program, fragShader);
        glLinkProgram(program);

        GLint status;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (!status) {
            std::array<char, 1000> buffer = { };

            int32_t length;
            glGetProgramInfoLog(program, buffer.size(), &length, buffer.data());

            fmt::print("Link Error: {}\n", std::string(buffer.data(), length));

            throw std::exception();
        }

        return program;
    }
}

Resource::Resource(Child *component) : component(component) { }

Child::Child(Engine &engine) : engine(engine) { }
Child::Child(Child *parent) : parent(parent), engine(parent->engine) { }

void Child::draw() { }
void Child::update(float time) { }
void Child::keyboard(int key, int action) { }

void Child::engineDraw() {
    draw();

    for (const auto &c : children) c->engineDraw();
}

void Child::engineUpdate(float time) {
    update(time);

    for (const auto &c : children) c->engineUpdate(time);
}

void Child::engineKeyboard(int key, int action) {
    keyboard(key, action);

    for (const auto &c : children) c->engineKeyboard(key, action);
}

void Engine::key(int key, int action) const {
    app->engineKeyboard(key, action);
}

void Engine::scale(int width, int height) const {
    constexpr float div = 80;

    glUniform2f(scaleUniform, static_cast<float>(width) / div, static_cast<float>(height) / div);
}

Resource *Child::find(size_t hash) {
    auto x = resources.find(hash);

    return x == resources.end() ? (parent ? parent->find(hash) : nullptr) : (x->second.get());
}

namespace {
    int64_t now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
}

void Engine::execute() {
    int64_t last = now();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClearColor(sky.red, sky.green, sky.blue, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        auto current = now();

        app->engineUpdate(static_cast<float>(current - last) / 1000.0f);

        glUniform2f(offsetUniform, offsetX, offsetY);

        world.Step(1 / 60.0f, 6, 2);
        app->engineDraw();

        last = current;

        glfwSwapBuffers(window);
    }
}

Engine::Engine(GLFWwindow *window) : window(window), world(b2Vec2(0.0f, -10.0f)), sky(0xCCFCF8) {
    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window))->key(key, action);
    });

    glfwSetWindowSizeCallback(window, [](GLFWwindow *window, int width, int height) {
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window))->scale(width, height);
    });

    Handle program = loadShaderProgram();
    glUseProgram(program);

    offsetUniform = glGetUniformLocation(program, "offset");
    scaleUniform = glGetUniformLocation(program, "scale");
    assert(offsetUniform >= 0 && scaleUniform >= 0);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    scale(width, height);
}
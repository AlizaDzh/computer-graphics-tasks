#include "asr.h"

[[noreturn]] int main()
{
    using namespace asr;

    auto window = std::make_shared<ES2SDLWindow>("asr");

    std::vector<std::shared_ptr<Object>> objects{};
    auto scene = std::make_shared<Scene>(objects);

    ES2Renderer renderer(scene, window);
    for (;;)
    {
        window->poll();

        bool show_demo_window = true;
        ImGui::ShowDemoWindow(&show_demo_window);

        renderer.render();
    }
}

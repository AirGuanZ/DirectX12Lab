#include <iostream>

#include <agz/d3d12/lab.h>

using namespace agz::d3d12;

void run()
{
    WindowDesc windowDesc;
    windowDesc.title = L"09.particles";

    Window window(windowDesc);

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        if(window.getKeyboard()->isDown(KEY_ESCAPE))
            window.setCloseFlag(true);
    }

    window.waitCommandQueueIdle();
}

int main()
{
    try
    {
        run();
    }
    catch(const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }
}

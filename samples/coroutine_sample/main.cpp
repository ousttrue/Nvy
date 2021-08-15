#include <Windows.h>

auto CLASS_NAME = L"COROUTINE_SAMPLE_CLASS";
auto TITLE_NAME = L"COROUTINE_SAMPLE";

#include "window.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR p_cmd_line, int n_cmd_show)
{
    Window window;
    auto hwnd = window.Create(instance, CLASS_NAME, TITLE_NAME);
    if (!hwnd)
    {
        return 1;
    }
    ShowWindow(hwnd, n_cmd_show);
    UpdateWindow(hwnd);

    while (window.NewFrame())
    {
    }

    return 0;
}

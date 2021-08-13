#pragma once
#include <string>
#include <list>
#include <functional>
#include <string>

enum class WindowEventTypes
{
    SizeChanged,
    DpiChanged,
    KeyChar,
    KeySysChar,
    KeyModified,
    MouseMove,
    MouseLeftDown,
    MouseRightDown,
    MouseMiddleDown,
    MouseLeftUp,
    MouseRightUp,
    MouseMiddleUp,
    MouseWheel,
    FileDroped,
};

struct WindowEvent
{
    WindowEventTypes type;
    union
    {
        struct
        {
            uint32_t width;
            uint32_t height;
        };
        uint32_t dpi;
        const wchar_t *path;
        struct
        {
            POINT client_point;
            short scroll_amount;
            bool should_resize_font;
        };
        POINTS cursor_pos;
        wchar_t key;
        const char *modified;
    };
};

using WindowEventCallback = std::function<void(const WindowEvent &)>;

class Win32Window
{
    HINSTANCE _instance = nullptr;
    std::wstring _className;
    HWND _hwnd = nullptr;
    std::list<WindowEventCallback> _callbacks;
    UINT _saved_dpi_scaling = 0;
    bool _dead_char_pending = false;
    WINDOWPLACEMENT _saved_window_placement;

public:
    Win32Window(HINSTANCE instance);
    ~Win32Window();
    HWND Create(const wchar_t *window_class_name, const wchar_t *window_title);
    LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    bool ProcessMessage();
    void ToggleFullscreen();
    void OnEvent(const std::function<void(const WindowEvent &)> &callback);

private:
    void RaiseEvent(const WindowEvent &event);
};

#pragma once
#include <stdint.h>
#include <Windows.h>
#include <vector>

enum NvimRequest : uint8_t
{
    vim_get_api_info = 0,
    nvim_input = 1,
    nvim_input_mouse = 2,
    nvim_eval = 3,
    nvim_command = 4
};
constexpr const char *NVIM_REQUEST_NAMES[]{"nvim_get_api_info", "nvim_input",
                                           "nvim_input_mouse", "nvim_eval",
                                           "nvim_command"};
enum NvimOutboundNotification : uint8_t
{
    nvim_ui_attach = 0,
    nvim_ui_try_resize = 1,
    nvim_set_var = 2
};
constexpr const char *NVIM_OUTBOUND_NOTIFICATION_NAMES[]{
    "nvim_ui_attach", "nvim_ui_try_resize", "nvim_set_var"};
enum class MouseButton
{
    Left,
    Right,
    Middle,
    Wheel
};
enum class MouseAction
{
    Press,
    Drag,
    Release,
    MouseWheelUp,
    MouseWheelDown,
    MouseWheelLeft,
    MouseWheelRight
};
constexpr int MAX_MPACK_OUTBOUND_MESSAGE_SIZE = 4096;

class Nvim
{
    class NvimImpl *_impl;

public:
    Nvim(wchar_t *command_line, HWND hwnd);
    ~Nvim();
    Nvim(const Nvim &) = delete;
    Nvim &operator=(const Nvim &) = delete;

    std::vector<char> ParseConfig(struct mpack_node_t *config_node);
    void SendUIAttach(int grid_rows, int grid_cols);
    void SendResize(int grid_rows, int grid_cols);
    void SendChar(wchar_t input_char);
    void SendSysChar(wchar_t sys_char);
    void SendInput(const char *input_chars);
    void SendInput(int virtual_key, int flags);
    void SendMouseInput(MouseButton button, MouseAction action, int mouse_row,
                        int mouse_col);
    bool ProcessKeyDown(int virtual_key);
    void OpenFile(const wchar_t *file_name);
    NvimRequest GetRequestFromID(size_t id) const;
};

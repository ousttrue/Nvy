#pragma once
#include <stdint.h>
#include <memory>
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

using NvimMessage = std::shared_ptr<struct mpack_tree_t>;

class NvimPipe
{
    class NvimImpl *_impl;

public:
    NvimPipe(wchar_t *command_line);
    ~NvimPipe();
    NvimPipe(const NvimPipe &) = delete;
    NvimPipe &operator=(const NvimPipe &) = delete;
    void Send(void *data, size_t size);
    int64_t RegisterRequest(NvimRequest request);
    NvimRequest GetRequestFromID(size_t id) const;
    bool TryDequeue(NvimMessage *msg);
};

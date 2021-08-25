#pragma once
#include <stdint.h>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <Windows.h>

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
public:
    HANDLE stdin_read = nullptr;
    HANDLE stdin_write = nullptr;
    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    PROCESS_INFORMATION process_info = {0};

private:
    int64_t next_msg_id = 0;
    std::vector<NvimRequest> msg_id_to_method;

    std::queue<NvimMessage> _queue;
    std::mutex _mutex;

public:
    NvimPipe();
    ~NvimPipe();
    NvimPipe(const NvimPipe &) = delete;
    NvimPipe &operator=(const NvimPipe &) = delete;
    bool Launch(const wchar_t *command_line);

    void Send(void *data, size_t size);
    int64_t RegisterRequest(NvimRequest request);
    NvimRequest GetRequestFromID(size_t id) const;
    void Enqueue(const NvimMessage &msg);
    bool TryDequeue(NvimMessage *msg);
};

#include "nvim.h"
#include "common/mpack_helper.h"
#include "third_party/mpack/mpack.h"
#include "common/window_messages.h"

constexpr int Megabytes(int n)
{
    return 1024 * 1024 * n;
}

static size_t ReadFromNvim(mpack_tree_t *tree, char *buffer, size_t count)
{
    HANDLE nvim_stdout_read = mpack_tree_context(tree);
    DWORD bytes_read;
    BOOL success = ReadFile(nvim_stdout_read, buffer, static_cast<DWORD>(count),
                            &bytes_read, nullptr);
    if (!success)
    {
        mpack_tree_flag_error(tree, mpack_error_io);
    }
    return bytes_read;
}

class NvimImpl
{
    HWND hwnd = nullptr;

    HANDLE stdin_read = nullptr;
    HANDLE stdin_write = nullptr;
    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    PROCESS_INFORMATION process_info = {0};

    int64_t next_msg_id = 0;
    Vec<NvimRequest> msg_id_to_method;

public:
    static DWORD WINAPI NvimMessageHandler(LPVOID param)
    {
        auto nvim = static_cast<NvimImpl *>(param);
        return nvim->MessageHandler();
    }

    static DWORD WINAPI NvimProcessMonitor(LPVOID param)
    {
        auto nvim = static_cast<NvimImpl *>(param);
        return nvim->ProcessMonitor();
    }

    NvimImpl(HWND hwnd, wchar_t *command_line) : hwnd(hwnd)
    {
        HANDLE job_object = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info{
            .BasicLimitInformation = JOBOBJECT_BASIC_LIMIT_INFORMATION{
                .LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE}};
        SetInformationJobObject(job_object, JobObjectExtendedLimitInformation,
                                &job_info, sizeof(job_info));

        SECURITY_ATTRIBUTES sec_attribs{.nLength = sizeof(SECURITY_ATTRIBUTES),
                                        .bInheritHandle = true};
        CreatePipe(&this->stdin_read, &this->stdin_write, &sec_attribs, 0);
        CreatePipe(&this->stdout_read, &this->stdout_write, &sec_attribs, 0);

        STARTUPINFO startup_info{.cb = sizeof(STARTUPINFO),
                                 .dwFlags = STARTF_USESTDHANDLES,
                                 .hStdInput = this->stdin_read,
                                 .hStdOutput = this->stdout_write,
                                 .hStdError = this->stdout_write};

        // wchar_t command_line[] = L"nvim --embed";
        CreateProcessW(nullptr, command_line, nullptr, nullptr, true,
                       CREATE_NO_WINDOW, nullptr, nullptr, &startup_info,
                       &this->process_info);
        AssignProcessToJobObject(job_object, this->process_info.hProcess);

        DWORD _;
        CreateThread(nullptr, 0, NvimMessageHandler, this, 0, &_);
        CreateThread(nullptr, 0, NvimProcessMonitor, this, 0, &_);

        // Query api info
        char data[MAX_MPACK_OUTBOUND_MESSAGE_SIZE];
        mpack_writer_t writer;
        mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);
        MPackStartRequest(this->RegisterRequest(vim_get_api_info),
                          NVIM_REQUEST_NAMES[vim_get_api_info], &writer);
        mpack_start_array(&writer, 0);
        mpack_finish_array(&writer);
        size_t size = MPackFinishMessage(&writer);
        MPackSendData(this->stdin_write, data, size);

        // Set g:nvy global variable
        mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);
        MPackStartNotification(NVIM_OUTBOUND_NOTIFICATION_NAMES[nvim_set_var],
                               &writer);
        mpack_start_array(&writer, 2);
        mpack_write_cstr(&writer, "nvy");
        mpack_write_int(&writer, 1);
        mpack_finish_array(&writer);
        size = MPackFinishMessage(&writer);
        MPackSendData(this->stdin_write, data, size);

        // Query stdpath to find the users init.vim
        mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);
        MPackStartRequest(this->RegisterRequest(nvim_eval),
                          NVIM_REQUEST_NAMES[nvim_eval], &writer);
        mpack_start_array(&writer, 1);
        mpack_write_cstr(&writer, "stdpath('config')");
        mpack_finish_array(&writer);
        size = MPackFinishMessage(&writer);
        MPackSendData(this->stdin_write, data, size);
    }

    ~NvimImpl()
    {
        DWORD exit_code;
        GetExitCodeProcess(this->process_info.hProcess, &exit_code);

        if (exit_code == STILL_ACTIVE)
        {
            CloseHandle(this->stdin_read);
            CloseHandle(this->stdin_write);
            CloseHandle(this->stdout_read);
            CloseHandle(this->stdout_write);
            CloseHandle(this->process_info.hThread);
            TerminateProcess(this->process_info.hProcess, 0);
            CloseHandle(this->process_info.hProcess);
        }
    }

    NvimRequest GetRequestFromID(size_t id) const
    {
        assert(id <= this->next_msg_id);
        return this->msg_id_to_method[id];
    }

    int64_t RegisterRequest(NvimRequest request)
    {
        msg_id_to_method.push_back(request);
        return next_msg_id++;
    }

    void NvimSendModifiedInput(const char *input, bool virtual_key)
    {
        bool shift_down = (GetKeyState(VK_SHIFT) & 0x80) != 0;
        bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x80) != 0;
        bool alt_down = (GetKeyState(VK_MENU) & 0x80) != 0;

        constexpr int MAX_INPUT_STRING_SIZE = 64;
        char input_string[MAX_INPUT_STRING_SIZE];

        snprintf(input_string, MAX_INPUT_STRING_SIZE, "<%s%s%s%s>",
                 ctrl_down ? "C-" : "", shift_down ? "S-" : "",
                 alt_down ? "M-" : "", input);

        char data[MAX_MPACK_OUTBOUND_MESSAGE_SIZE];
        mpack_writer_t writer;
        mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);
        MPackStartRequest(RegisterRequest(nvim_input),
                          NVIM_REQUEST_NAMES[nvim_input], &writer);
        mpack_start_array(&writer, 1);
        mpack_write_cstr(&writer, input_string);
        mpack_finish_array(&writer);
        size_t size = MPackFinishMessage(&writer);
        MPackSendData(this->stdin_write, data, size);
    }

    DWORD MessageHandler()
    {
        mpack_tree_t *tree =
            static_cast<mpack_tree_t *>(malloc(sizeof(mpack_tree_t)));
        mpack_tree_init_stream(tree, ReadFromNvim, this->stdout_read,
                               Megabytes(20), 1024 * 1024);

        while (true)
        {
            mpack_tree_parse(tree);
            if (mpack_tree_error(tree) != mpack_ok)
            {
                break;
            }

            // Blocking, dubious thread safety. Seems to work though...
            SendMessage(this->hwnd, WM_NVIM_MESSAGE,
                        reinterpret_cast<WPARAM>(tree), 0);
        }

        mpack_tree_destroy(tree);
        free(tree);
        PostMessage(this->hwnd, WM_DESTROY, 0, 0);
        return 0;
    }

    DWORD ProcessMonitor()
    {
        while (true)
        {
            DWORD exit_code;
            if (GetExitCodeProcess(this->process_info.hProcess, &exit_code) &&
                exit_code == STILL_ACTIVE)
            {
                Sleep(1);
            }
            else
            {
                break;
            }
        }
        PostMessage(this->hwnd, WM_DESTROY, 0, 0);
        return 0;
    }

    void Send(void *data, size_t size)
    {
        MPackSendData(this->stdin_write, data, size);
    }
};

Nvim::Nvim(wchar_t *command_line, HWND hwnd)
    : _impl(new NvimImpl(hwnd, command_line))
{
}

Nvim::~Nvim()
{
    delete _impl;
}

std::vector<char> Nvim::ParseConfig(mpack_node_t *config_node)
{
    std::vector<char> guifont_out;
    char path[MAX_PATH];
    const char *config_path = mpack_node_str(*config_node);
    size_t config_path_strlen = mpack_node_strlen(*config_node);
    strncpy_s(path, MAX_PATH, config_path, config_path_strlen);
    strcat_s(path, MAX_PATH - config_path_strlen - 1, "\\init.vim");

    HANDLE config_file =
        CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);

    if (config_file == INVALID_HANDLE_VALUE)
    {
        return guifont_out;
    }

    char *buffer;
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(config_file, &file_size))
    {
        CloseHandle(config_file);
        return guifont_out;
    }
    buffer = static_cast<char *>(malloc(file_size.QuadPart));

    DWORD bytes_read;
    if (!ReadFile(config_file, buffer, file_size.QuadPart, &bytes_read, NULL))
    {
        CloseHandle(config_file);
        free(buffer);
        return guifont_out;
    }
    CloseHandle(config_file);

    char *strtok_context;
    char *line = strtok_s(buffer, "\r\n", &strtok_context);
    while (line)
    {
        char *guifont = strstr(line, "set guifont=");
        if (guifont)
        {
            // Check if we're inside a comment
            int leading_count = guifont - line;
            bool inside_comment = false;
            for (int i = 0; i < leading_count; ++i)
            {
                if (line[i] == '"')
                {
                    inside_comment = !inside_comment;
                }
            }
            if (!inside_comment)
            {
                guifont_out.clear();

                int line_offset = (guifont - line + strlen("set guifont="));
                int guifont_strlen = strlen(line) - line_offset;
                int escapes = 0;
                for (int i = 0; i < guifont_strlen; ++i)
                {
                    if (line[line_offset + i] == '\\' &&
                        i < (guifont_strlen - 1) &&
                        line[line_offset + i + 1] == ' ')
                    {
                        guifont_out.push_back(' ');
                        ++i;
                        continue;
                    }
                    guifont_out.push_back(line[i + line_offset]);
                }
                guifont_out.push_back('\0');
            }
        }
        line = strtok_s(NULL, "\r\n", &strtok_context);
    }

    free(buffer);
    return guifont_out;
}

void Nvim::SendUIAttach(int grid_rows, int grid_cols)
{
    char data[MAX_MPACK_OUTBOUND_MESSAGE_SIZE];
    mpack_writer_t writer;

    // Send UI attach notification
    mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);
    MPackStartNotification(NVIM_OUTBOUND_NOTIFICATION_NAMES[nvim_ui_attach],
                           &writer);
    mpack_start_array(&writer, 3);
    mpack_write_int(&writer, grid_cols);
    mpack_write_int(&writer, grid_rows);
    mpack_start_map(&writer, 1);
    mpack_write_cstr(&writer, "ext_linegrid");
    mpack_write_true(&writer);
    mpack_finish_map(&writer);
    mpack_finish_array(&writer);
    size_t size = MPackFinishMessage(&writer);

    _impl->Send(data, size);
}

void Nvim::SendResize(int grid_rows, int grid_cols)
{
    char data[MAX_MPACK_OUTBOUND_MESSAGE_SIZE];
    mpack_writer_t writer;
    mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);

    MPackStartNotification(NVIM_OUTBOUND_NOTIFICATION_NAMES[nvim_ui_try_resize],
                           &writer);
    mpack_start_array(&writer, 2);
    mpack_write_int(&writer, grid_cols);
    mpack_write_int(&writer, grid_rows);
    mpack_finish_array(&writer);
    size_t size = MPackFinishMessage(&writer);

    _impl->Send(data, size);
}

void Nvim::SendChar(wchar_t input_char)
{
    // If the space is simply a regular space,
    // simply send the modified input
    if (input_char == VK_SPACE)
    {
        _impl->NvimSendModifiedInput("Space", true);
        return;
    }

    char utf8_encoded[64]{};
    if (!WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, 0, 0, NULL, NULL))
    {
        return;
    }
    WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, utf8_encoded, 64, NULL,
                        NULL);

    char data[MAX_MPACK_OUTBOUND_MESSAGE_SIZE];
    mpack_writer_t writer;
    mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);
    MPackStartRequest(_impl->RegisterRequest(nvim_input),
                      NVIM_REQUEST_NAMES[nvim_input], &writer);
    mpack_start_array(&writer, 1);
    mpack_write_cstr(&writer, utf8_encoded);
    mpack_finish_array(&writer);
    size_t size = MPackFinishMessage(&writer);

    _impl->Send(data, size);
}

void Nvim::SendSysChar(wchar_t input_char)
{
    char utf8_encoded[64]{};
    if (!WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, 0, 0, NULL, NULL))
    {
        return;
    }
    WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, utf8_encoded, 64, NULL,
                        NULL);

    _impl->NvimSendModifiedInput(utf8_encoded, true);
}

void Nvim::SendInput(const char *input_chars)
{
    char data[MAX_MPACK_OUTBOUND_MESSAGE_SIZE];
    mpack_writer_t writer;
    mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);

    MPackStartRequest(_impl->RegisterRequest(nvim_input),
                      NVIM_REQUEST_NAMES[nvim_input], &writer);
    mpack_start_array(&writer, 1);
    mpack_write_cstr(&writer, input_chars);
    mpack_finish_array(&writer);
    size_t size = MPackFinishMessage(&writer);
    _impl->Send(data, size);
}

void Nvim::SendMouseInput(MouseButton button, MouseAction action, int mouse_row,
                          int mouse_col)
{
    char data[MAX_MPACK_OUTBOUND_MESSAGE_SIZE];
    mpack_writer_t writer;
    mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);
    MPackStartRequest(_impl->RegisterRequest(nvim_input_mouse),
                      NVIM_REQUEST_NAMES[nvim_input_mouse], &writer);
    mpack_start_array(&writer, 6);

    switch (button)
    {
    case MouseButton::Left:
    {
        mpack_write_cstr(&writer, "left");
    }
    break;
    case MouseButton::Right:
    {
        mpack_write_cstr(&writer, "right");
    }
    break;
    case MouseButton::Middle:
    {
        mpack_write_cstr(&writer, "middle");
    }
    break;
    case MouseButton::Wheel:
    {
        mpack_write_cstr(&writer, "wheel");
    }
    break;
    }
    switch (action)
    {
    case MouseAction::Press:
    {
        mpack_write_cstr(&writer, "press");
    }
    break;
    case MouseAction::Drag:
    {
        mpack_write_cstr(&writer, "drag");
    }
    break;
    case MouseAction::Release:
    {
        mpack_write_cstr(&writer, "release");
    }
    break;
    case MouseAction::MouseWheelUp:
    {
        mpack_write_cstr(&writer, "up");
    }
    break;
    case MouseAction::MouseWheelDown:
    {
        mpack_write_cstr(&writer, "down");
    }
    break;
    case MouseAction::MouseWheelLeft:
    {
        mpack_write_cstr(&writer, "left");
    }
    break;
    case MouseAction::MouseWheelRight:
    {
        mpack_write_cstr(&writer, "right");
    }
    break;
    }

    bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x80) != 0;
    bool shift_down = (GetKeyState(VK_SHIFT) & 0x80) != 0;
    bool alt_down = (GetKeyState(VK_MENU) & 0x80) != 0;
    constexpr int MAX_INPUT_STRING_SIZE = 64;
    char input_string[MAX_INPUT_STRING_SIZE];
    snprintf(input_string, MAX_INPUT_STRING_SIZE, "%s%s%s",
             ctrl_down ? "C-" : "", shift_down ? "S-" : "",
             alt_down ? "M-" : "");
    mpack_write_cstr(&writer, input_string);

    mpack_write_i64(&writer, 0);
    mpack_write_i64(&writer, mouse_row);
    mpack_write_i64(&writer, mouse_col);
    mpack_finish_array(&writer);

    size_t size = MPackFinishMessage(&writer);
    _impl->Send(data, size);
}

bool Nvim::ProcessKeyDown(int virtual_key)
{
    const char *key;
    switch (virtual_key)
    {
    case VK_BACK:
    {
        key = "BS";
    }
    break;
    case VK_TAB:
    {
        key = "Tab";
    }
    break;
    case VK_RETURN:
    {
        key = "CR";
    }
    break;
    case VK_ESCAPE:
    {
        key = "Esc";
    }
    break;
    case VK_PRIOR:
    {
        key = "PageUp";
    }
    break;
    case VK_NEXT:
    {
        key = "PageDown";
    }
    break;
    case VK_HOME:
    {
        key = "Home";
    }
    break;
    case VK_END:
    {
        key = "End";
    }
    break;
    case VK_LEFT:
    {
        key = "Left";
    }
    break;
    case VK_UP:
    {
        key = "Up";
    }
    break;
    case VK_RIGHT:
    {
        key = "Right";
    }
    break;
    case VK_DOWN:
    {
        key = "Down";
    }
    break;
    case VK_INSERT:
    {
        key = "Insert";
    }
    break;
    case VK_DELETE:
    {
        key = "Del";
    }
    break;
    case VK_NUMPAD0:
    {
        key = "k0";
    }
    break;
    case VK_NUMPAD1:
    {
        key = "k1";
    }
    break;
    case VK_NUMPAD2:
    {
        key = "k2";
    }
    break;
    case VK_NUMPAD3:
    {
        key = "k3";
    }
    break;
    case VK_NUMPAD4:
    {
        key = "k4";
    }
    break;
    case VK_NUMPAD5:
    {
        key = "k5";
    }
    break;
    case VK_NUMPAD6:
    {
        key = "k6";
    }
    break;
    case VK_NUMPAD7:
    {
        key = "k7";
    }
    break;
    case VK_NUMPAD8:
    {
        key = "k8";
    }
    break;
    case VK_NUMPAD9:
    {
        key = "k9";
    }
    break;
    case VK_MULTIPLY:
    {
        key = "kMultiply";
    }
    break;
    case VK_ADD:
    {
        key = "kPlus";
    }
    break;
    case VK_SEPARATOR:
    {
        key = "kComma";
    }
    break;
    case VK_SUBTRACT:
    {
        key = "kMinus";
    }
    break;
    case VK_DECIMAL:
    {
        key = "kPoint";
    }
    break;
    case VK_DIVIDE:
    {
        key = "kDivide";
    }
    break;
    case VK_F1:
    {
        key = "F1";
    }
    break;
    case VK_F2:
    {
        key = "F2";
    }
    break;
    case VK_F3:
    {
        key = "F3";
    }
    break;
    case VK_F4:
    {
        key = "F4";
    }
    break;
    case VK_F5:
    {
        key = "F5";
    }
    break;
    case VK_F6:
    {
        key = "F6";
    }
    break;
    case VK_F7:
    {
        key = "F7";
    }
    break;
    case VK_F8:
    {
        key = "F8";
    }
    break;
    case VK_F9:
    {
        key = "F9";
    }
    break;
    case VK_F10:
    {
        key = "F10";
    }
    break;
    case VK_F11:
    {
        key = "F11";
    }
    break;
    case VK_F12:
    {
        key = "F12";
    }
    break;
    case VK_F13:
    {
        key = "F13";
    }
    break;
    case VK_F14:
    {
        key = "F14";
    }
    break;
    case VK_F15:
    {
        key = "F15";
    }
    break;
    case VK_F16:
    {
        key = "F16";
    }
    break;
    case VK_F17:
    {
        key = "F17";
    }
    break;
    case VK_F18:
    {
        key = "F18";
    }
    break;
    case VK_F19:
    {
        key = "F19";
    }
    break;
    case VK_F20:
    {
        key = "F20";
    }
    break;
    case VK_F21:
    {
        key = "F21";
    }
    break;
    case VK_F22:
    {
        key = "F22";
    }
    break;
    case VK_F23:
    {
        key = "F23";
    }
    break;
    case VK_F24:
    {
        key = "F24";
    }
    break;
    default:
    {
    }
        return false;
    }

    _impl->NvimSendModifiedInput(key, true);
    return true;
}

void Nvim::OpenFile(const wchar_t *file_name)
{
    char utf8_encoded[MAX_PATH]{};
    WideCharToMultiByte(CP_UTF8, 0, file_name, -1, utf8_encoded, MAX_PATH, NULL,
                        NULL);

    char file_command[MAX_PATH + 2] = {};
    strcpy_s(file_command, MAX_PATH, "e ");
    strcat_s(file_command, MAX_PATH - 3, utf8_encoded);

    char data[MAX_MPACK_OUTBOUND_MESSAGE_SIZE];
    mpack_writer_t writer;
    mpack_writer_init(&writer, data, MAX_MPACK_OUTBOUND_MESSAGE_SIZE);
    MPackStartRequest(_impl->RegisterRequest(nvim_command),
                      NVIM_REQUEST_NAMES[nvim_command], &writer);
    mpack_start_array(&writer, 1);
    mpack_write_cstr(&writer, file_command);
    mpack_finish_array(&writer);
    size_t size = MPackFinishMessage(&writer);
    _impl->Send(data, size);
}

NvimRequest Nvim::GetRequestFromID(size_t id) const
{
    return _impl->GetRequestFromID(id);
}

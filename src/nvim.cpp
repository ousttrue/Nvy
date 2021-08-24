#include "nvim.h"
#include "mpack.h"
#include "mpack_helper.h"
#include "window_messages.h"
#include <plog/Log.h>
#include <msgpackpp/msgpackpp.h>
#include <stdint.h>
#include <queue>
#include <mutex>
#include <Windows.h>

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
    HANDLE stdin_read = nullptr;
    HANDLE stdin_write = nullptr;
    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    PROCESS_INFORMATION process_info = {0};

    int64_t next_msg_id = 0;
    std::vector<NvimRequest> msg_id_to_method;

    std::queue<NvimMessage> _queue;
    std::mutex _mutex;

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

    NvimImpl(wchar_t *command_line)
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

    void Enqueue(const NvimMessage &msg)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _queue.push(msg);
    }
    bool TryDequeue(NvimMessage *msg)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_queue.empty())
        {
            return false;
        }

        *msg = _queue.front();
        _queue.pop();
        return true;
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

    DWORD MessageHandler()
    {
        while (true)
        {
            auto tree = NvimMessage(
                static_cast<mpack_tree_t *>(malloc(sizeof(mpack_tree_t))),
                [](mpack_tree_t *p)
                {
                    mpack_tree_destroy(p);
                    free(p);
                });
            mpack_tree_init_stream(tree.get(), ReadFromNvim, this->stdout_read,
                                   Megabytes(20), 1024 * 1024);

            mpack_tree_parse(tree.get());
            if (mpack_tree_error(tree.get()) != mpack_ok)
            {
                break;
            }

            auto u = msgpackpp::parser((const uint8_t *)tree->data,
                                       tree->data_length);
            switch (u[0].get_number<int>())
            {
            case 0:
                // request [0, msgid, method, params]
                PLOG_DEBUG << "[request#" << u[1].get_number<int>() << ", "
                           << u[2].get_string() << "]";
                break;
            case 1:
                // response [1, msgid, error, result]
                PLOG_DEBUG << "[response#" << u[1].get_number<int>() << "]";
                break;
            case 2:
                // notify [2, method, params]
                PLOG_DEBUG << "[notify, " << u[1].get_string() << "]";
                break;
            default:
                assert(false);
                break;
            }

            Enqueue(tree);
        }

        Enqueue(nullptr);
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
        Enqueue(nullptr);
        return 0;
    }

    void Send(void *data, size_t size)
    {
        MPackSendData(this->stdin_write, data, size);
    }
};

Nvim::Nvim(wchar_t *command_line) : _impl(new NvimImpl(command_line))
{
}

Nvim::~Nvim()
{
    delete _impl;
}

void Nvim::Send(void *data, size_t size)
{
    _impl->Send(data, size);
}

int64_t Nvim::RegisterRequest(NvimRequest request)
{
    return _impl->RegisterRequest(request);
}

NvimRequest Nvim::GetRequestFromID(size_t id) const
{
    return _impl->GetRequestFromID(id);
}

bool Nvim::TryDequeue(NvimMessage *msg)
{
    return _impl->TryDequeue(msg);
}

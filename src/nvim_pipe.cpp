#include "nvim_pipe.h"
#include "mpack.h"
#include "mpack_helper.h"
#include "window_messages.h"
#include <plog/Log.h>
#include <msgpackpp/msgpackpp.h>
#include <stdint.h>

static DWORD WINAPI NvimMessageHandler(LPVOID param)
{
    auto nvim = (NvimPipe *)param;

    std::vector<uint8_t> buffer(1024 * 1024);
    while (true)
    {
        DWORD bytes_read;
        BOOL success =
            ReadFile(nvim->_stdout_read, buffer.data(),
                     static_cast<DWORD>(buffer.size()), &bytes_read, nullptr);
        if (!success)
        {
            break;
        }

        if (bytes_read)
        {
            nvim->Enqueue({buffer.data(), buffer.data() + bytes_read});
        }
    }

    nvim->Enqueue({});
    return 0;
}

static DWORD WINAPI NvimProcessMonitor(LPVOID param)
{
    auto nvim = static_cast<NvimPipe *>(param);
    while (true)
    {
        DWORD exit_code;
        if (GetExitCodeProcess(nvim->_process_info.hProcess, &exit_code) &&
            exit_code == STILL_ACTIVE)
        {
            Sleep(1);
        }
        else
        {
            break;
        }
    }
    nvim->Enqueue({});
    return 0;
}

NvimPipe::NvimPipe()
{
}

NvimPipe::~NvimPipe()
{
    DWORD exit_code;
    GetExitCodeProcess(_process_info.hProcess, &exit_code);

    if (exit_code == STILL_ACTIVE)
    {
        CloseHandle(_stdin_read);
        CloseHandle(_stdin_write);
        CloseHandle(_stdout_read);
        CloseHandle(_stdout_write);
        CloseHandle(_process_info.hThread);
        TerminateProcess(_process_info.hProcess, 0);
        CloseHandle(_process_info.hProcess);
    }
}

bool NvimPipe::Launch(const wchar_t *command_line)
{
    SECURITY_ATTRIBUTES sec_attribs{.nLength = sizeof(SECURITY_ATTRIBUTES),
                                    .bInheritHandle = true};
    if (!CreatePipe(&_stdin_read, &_stdin_write, &sec_attribs, 0))
    {
        return false;
    }
    if (!CreatePipe(&_stdout_read, &_stdout_write, &sec_attribs, 0))
    {
        return false;
    }

    STARTUPINFO startup_info{.cb = sizeof(STARTUPINFO),
                             .dwFlags = STARTF_USESTDHANDLES,
                             .hStdInput = _stdin_read,
                             .hStdOutput = _stdout_write,
                             .hStdError = _stdout_write};

    // wchar_t command_line[] = L"nvim --embed";
    if (!CreateProcessW(nullptr, std::wstring(command_line).data(), nullptr,
                        nullptr, true, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup_info, &_process_info))
    {
        return false;
    }

    HANDLE job_object = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info{
        .BasicLimitInformation = JOBOBJECT_BASIC_LIMIT_INFORMATION{
            .LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE}};
    SetInformationJobObject(job_object, JobObjectExtendedLimitInformation,
                            &job_info, sizeof(job_info));
    AssignProcessToJobObject(job_object, _process_info.hProcess);

    DWORD _;
    CreateThread(nullptr, 0, NvimMessageHandler, this, 0, &_);
    CreateThread(nullptr, 0, NvimProcessMonitor, this, 0, &_);

    return true;
}

void NvimPipe::Send(const void *data, size_t size)
{
    auto u = msgpackpp::parser((const uint8_t *)data, size);
    PLOG_DEBUG << u.to_json();

    DWORD bytes_written;
    bool success = WriteFile(_stdin_write, data, static_cast<DWORD>(size),
                             &bytes_written, nullptr);
    if (!success)
    {
        assert(false);
    }
}

int64_t NvimPipe::RegisterRequest(NvimRequest request)
{
    _msg_id_to_method.push_back(request);
    return _next_msg_id++;
}

NvimRequest NvimPipe::GetRequestFromID(size_t id) const
{
    assert(id <= _next_msg_id);
    return _msg_id_to_method[id];
}

void NvimPipe::Enqueue(const NvimMessage &msg)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _queue.push(msg);
}

bool NvimPipe::TryDequeue(NvimMessage *msg)
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

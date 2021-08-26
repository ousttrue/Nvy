#include "nvim_pipe.h"
#include "window_messages.h"
#include <Windows.h>
#include <msgpackpp/msgpackpp.h>
#include <plog/Log.h>
#include <stdint.h>
#include <thread>

static HANDLE _stdin_read = nullptr;
static HANDLE _stdin_write = nullptr;
void *NvimPipe::WriteHandle() { return _stdin_write; }
static HANDLE _stdout_read = nullptr;
void *NvimPipe::ReadHandle() { return _stdout_read; }
static HANDLE _stdout_write = nullptr;
static PROCESS_INFORMATION _process_info = {0};

std::thread _watch;

NvimPipe::NvimPipe() {}

NvimPipe::~NvimPipe() {
  DWORD exit_code;
  GetExitCodeProcess(_process_info.hProcess, &exit_code);

  if (exit_code == STILL_ACTIVE) {
    if (_watch.joinable()) {
      _watch.join();
    }

    CloseHandle(_stdin_read);
    CloseHandle(_stdin_write);
    CloseHandle(_stdout_read);
    CloseHandle(_stdout_write);
    CloseHandle(_process_info.hThread);
    TerminateProcess(_process_info.hProcess, 0);
    WaitForSingleObject(_process_info.hProcess, INFINITE);
    CloseHandle(_process_info.hProcess);
  }
}

bool NvimPipe::Launch(const wchar_t *command_line) {
  SECURITY_ATTRIBUTES sec_attribs{.nLength = sizeof(SECURITY_ATTRIBUTES),
                                  .bInheritHandle = true};
  if (!CreatePipe(&_stdin_read, &_stdin_write, &sec_attribs, 0)) {
    return false;
  }
  if (!CreatePipe(&_stdout_read, &_stdout_write, &sec_attribs, 0)) {
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
                      &startup_info, &_process_info)) {
    return false;
  }
  _watch = std::thread([]() {
    WaitForSingleObject(_process_info.hProcess, INFINITE);
    CancelSynchronousIo(_stdout_read);
    CloseHandle(_process_info.hProcess);
  });

  HANDLE job_object = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info{
      .BasicLimitInformation = JOBOBJECT_BASIC_LIMIT_INFORMATION{
          .LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE}};
  SetInformationJobObject(job_object, JobObjectExtendedLimitInformation,
                          &job_info, sizeof(job_info));
  AssignProcessToJobObject(job_object, _process_info.hProcess);

  return true;
}

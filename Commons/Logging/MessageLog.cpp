//
// Copyright(c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "MessageLog.h"
#include "..\Utility\ProcessHelper.h"
#include "..\Utility\StringUtils.h"

#include <Windows.h>
#include <chrono>
#include <ctime>
#include <iomanip>

MessageLog g_messageLog;

MessageLog::MessageLog() 
  : filter_({ LogLevel::LOG_ERROR, LogLevel::LOG_INFO, LogLevel::LOG_WARNING }),
  started_(false), caller_("")
{
  parentProcess_ = std::to_string(GetCurrentProcessId()) + " " + ConvertUTF16StringToUTF8String(GetProcessNameFromHandle(GetCurrentProcess()));
}

MessageLog::~MessageLog() { outFile_.close(); }
void MessageLog::Start(const std::wstring& logFilePath, const std::wstring& caller, bool overwrite)
{
  Start(ConvertUTF16StringToUTF8String(logFilePath), ConvertUTF16StringToUTF8String(caller),
        overwrite);
}

void MessageLog::Start(const std::string& logFilePath, const std::string& caller, bool overwrite)
{
  auto openMode = overwrite ? std::ofstream::out : std::ofstream::app;
  outFile_.open(logFilePath + ".txt", openMode);
  if (!outFile_.is_open()) {
    const std::string message = "Unable to open logFile " + logFilePath + " for " + caller;
    MessageBoxA(NULL, message.c_str(), NULL, MB_OK);
  }
  caller_ = caller;
  started_ = true;
  Log(LOG_INFO, "MessageLog", "Logging started");
}

std::string MessageLog::CreateLogMessage(LogLevel logLevel, const std::string& category, const std::string& message,
  DWORD errorCode) 
{
  SetCurrentTime();
  std::ostringstream outstream;
  outstream << std::put_time(&currentTime_, "%c") << "\t";

  outstream << std::left << std::setw(12) << std::setfill(' ');
  outstream << logLevelNames_[logLevel];
  if (started_) {
    outstream << caller_ << " ";
  }
  outstream << parentProcess_;
  outstream << " - " << category;
  outstream << " - " << message;
  if (errorCode) {
    auto systemErrorMessage = GetSystemErrorMessage(errorCode);
    outstream << " - " << " Error Code: " << errorCode << " (" << systemErrorMessage << ")";
  }
  return outstream.str();
}

void MessageLog::Log(LogLevel logLevel, const std::string& category, const std::string& message,
                     DWORD errorCode)
{
  // Filter the message
  if (filter_.find(logLevel) == filter_.end()) {
    return;
  }

  const auto logMessage = CreateLogMessage(logLevel, category, message, errorCode);
  if (started_ && outFile_.is_open()) {
    outFile_ << logMessage << std::endl;
    outFile_.flush();
  }

  // Always print to debug console
  std::wstring debugOutput = L"OCAT: " + ConvertUTF8StringToUTF16String(logMessage) + L"\n";
  OutputDebugString(debugOutput.c_str());
}

void MessageLog::Log(LogLevel logLevel, const std::string& category, const std::wstring& message,
                     DWORD errorCode)
{
  Log(logLevel, category, ConvertUTF16StringToUTF8String(message), errorCode);
}

typedef void(WINAPI* FGETSYSTEMINFO)(LPSYSTEM_INFO);
typedef BOOL(WINAPI* FGETPRODUCTINFO)(DWORD, DWORD, DWORD, DWORD, PDWORD);
void MessageLog::LogOS()
{
  SYSTEM_INFO systemInfo = {};
  FGETSYSTEMINFO fGetSystemInfo = reinterpret_cast<FGETSYSTEMINFO>(
      GetProcAddress(GetModuleHandle(L"kernel32.dll"), "GetNativeSystemInfo"));
  if (fGetSystemInfo) {
    fGetSystemInfo(&systemInfo);
  }
  else {
    g_messageLog.Log(MessageLog::LOG_WARNING, "MessageLog",
                     "LogOS: Unable to get address for GetNativeSystemInfo using GetSystemInfo");
    GetSystemInfo(&systemInfo);
  }

  std::string processorArchitecture = "Processor architecture: ";
  switch (systemInfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
      processorArchitecture += "x64";
      break;
    case PROCESSOR_ARCHITECTURE_INTEL:
      processorArchitecture += "x86";
      break;
    case PROCESSOR_ARCHITECTURE_ARM:
      processorArchitecture += "ARM";
      break;
    case PROCESSOR_ARCHITECTURE_IA64:
      processorArchitecture += "Intel Itanium";
      break;
    default:
      processorArchitecture += "Unknown";
      break;
  }

  FGETPRODUCTINFO fGetProductInfo = reinterpret_cast<FGETPRODUCTINFO>(
      GetProcAddress(GetModuleHandle(L"kernel32.dll"), "GetProductInfo"));
  if (!fGetProductInfo) {
    g_messageLog.Log(MessageLog::LOG_ERROR, "MessageLog",
                     "LogOS: Unable to get address for GetProductInfo");
    return;
  }

  DWORD type = 0;
  fGetProductInfo(6, 0, 0, 0, &type);
  const std::string osInfo = "OS: type: " + std::to_string(type);

  g_messageLog.Log(MessageLog::LOG_INFO, "MessageLog", osInfo + " " + processorArchitecture);
}

void MessageLog::SetCurrentTime()
{
  const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  localtime_s(&currentTime_, &time);
}
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <vector>

HANDLE FindSecondLargestNvContainerProcessHandle() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create process snapshot." << std::endl;
        return nullptr;
    }

    PROCESSENTRY32 entry = { sizeof(PROCESSENTRY32) };
    if (!Process32First(snapshot, &entry)) {
        std::cerr << "Failed to get first process." << std::endl;
        CloseHandle(snapshot);
        return nullptr;
    }

    DWORD firstMaxMemory = 0, secondMaxMemory = 0;
    HANDLE secondLargestHandle = nullptr;
    HANDLE firstLargestHandle = nullptr;

    do {
        if (std::wstring(entry.szExeFile) == L"nvcontainer.exe") {
            HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
            if (process) {
                PROCESS_MEMORY_COUNTERS pmc = {};
                if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
                    DWORD workingSetSize = pmc.WorkingSetSize;

                    if (workingSetSize > firstMaxMemory) {
                        secondMaxMemory = firstMaxMemory;
                        secondLargestHandle = firstLargestHandle;

                        firstMaxMemory = workingSetSize;
                        firstLargestHandle = process;
                    }
                    else if (workingSetSize > secondMaxMemory) {
                        secondMaxMemory = workingSetSize;
                        secondLargestHandle = process;
                    }
                    else {
                        CloseHandle(process);
                    }
                }
                else {
                    CloseHandle(process);
                }
            }
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);

    if (firstLargestHandle != nullptr && firstLargestHandle != secondLargestHandle) {
        CloseHandle(firstLargestHandle);
    }

    return secondLargestHandle;
}

bool IsNvContainerBelowThreshold(HANDLE processHandle, DWORD threshold) {
    PROCESS_MEMORY_COUNTERS pmc = {};
    if (GetProcessMemoryInfo(processHandle, &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize < threshold;
    }
    return false;
}

bool keyCombinationSent = false;

void SimulateKeyCombination() {
    if (keyCombinationSent) {
        return; // Проверка на отправление комбинации клавиш
    }

    INPUT inputs[6] = {};

    // Нажатие Alt
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_MENU;

    // Нажатие Shift
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_SHIFT;

    // Нажатие F10
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = VK_F10;

    // Отпускание F10
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_F10;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    // Отпускание Shift
    inputs[4].type = INPUT_KEYBOARD;
    inputs[4].ki.wVk = VK_SHIFT;
    inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;

    // Отпускание Alt
    inputs[5].type = INPUT_KEYBOARD;
    inputs[5].ki.wVk = VK_MENU;
    inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(6, inputs, sizeof(INPUT));
    keyCombinationSent = true; // Устанавливаем флаг отправленного сочетания клавиш
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Ждём 5 секунд перед сбросом флага
    keyCombinationSent = false; // Сбрасываем флаг после истечения времениd
}

void DisplayAsciiArtFromCode(int delay = 100) {
    const char* art = R"(
+===============================================================================================================+  
| _   ___     _____ ____ ___    _      ____  _____ ____  _        _ __   __   ___  _____ _____   _____ _____  __| 
|| \ | \ \   / /_ _|  _ \_ _|  / \    |  _ \| ____|  _ \| |      / \\ \ / /  / _ \|  ___|  ___| |  ___|_ _\ \/ /|
||  \| |\ \ / / | || | | | |  / _ \   | |_) |  _| | |_) | |     / _ \\ V /  | | | | |_  | |_    | |_   | | \  / |
|| |\  | \ V /  | || |_| | | / ___ \  |  _ <| |___|  __/| |___ / ___ \| |   | |_| |  _| |  _|   |  _|  | | /  \ |
||_| \_|  \_/  |___|____/___/_/   \_\ |_| \_\_____|_|   |_____/_/   \_\_|    \___/|_|   |_|     |_|   |___/_/\_\|
+===============================================================================================================+
)";

    // Вывод ASCII картинки
    const char* line = art;
    while (*line) {
        if (*line == '\n') {
            std::cout << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
        else {
            std::cout << *line;
        }
        ++line;
    }
}


int main() {

    int animationSpeed = 50; // Ускорение анимации: значение 50 мс между кадрами
    DisplayAsciiArtFromCode(animationSpeed); // Запуск анимации с ускорением

    while (true) {
        HANDLE processHandle = FindSecondLargestNvContainerProcessHandle();
        if (processHandle == nullptr) {
            std::cerr << "No valid nvcontainer.exe process found." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }

        if (IsNvContainerBelowThreshold(processHandle, 50000 * 1024)) {
            std::cout << "Attempting to enable Instant Replay..." << std::endl;
            SimulateKeyCombination();
        }

        CloseHandle(processHandle);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <algorithm>

// Глобальные переменные для управления временем отправки
std::chrono::steady_clock::time_point lastSentTime;
const std::chrono::seconds cooldownPeriod(60); // Кулдаун 60 секунд

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

    std::vector<std::pair<DWORD, HANDLE>> processes;

    do {
        if (std::wstring(entry.szExeFile) == L"nvcontainer.exe") {
            HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
            if (process) {
                PROCESS_MEMORY_COUNTERS pmc = {};
                if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
                    processes.emplace_back(pmc.WorkingSetSize, process);
                }
                else {
                    CloseHandle(process);
                }
            }
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);

    if (processes.size() < 2) {
        for (auto& p : processes) CloseHandle(p.second);
        return nullptr;
    }

    // Сортировка процессов по убыванию используемой памяти
    std::sort(processes.begin(), processes.end(), [](auto& a, auto& b) {
        return a.first > b.first;
        });

    // Закрываем все дескрипторы, кроме второго по величине
    for (size_t i = 0; i < processes.size(); ++i) {
        if (i != 1) CloseHandle(processes[i].second);
    }

    return processes.size() >= 2 ? processes[1].second : nullptr;
}

bool IsNvContainerBelowThreshold(HANDLE processHandle, DWORD threshold) {
    PROCESS_MEMORY_COUNTERS pmc = {};
    if (GetProcessMemoryInfo(processHandle, &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize < threshold;
    }
    return false;
}

void SimulateKeyCombination() {
    auto now = std::chrono::steady_clock::now();
    if (now - lastSentTime < cooldownPeriod) {
        return;
    }

    INPUT inputs[6] = {};

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_MENU;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_SHIFT;

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = VK_F10;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_F10;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[4].type = INPUT_KEYBOARD;
    inputs[4].ki.wVk = VK_SHIFT;
    inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[5].type = INPUT_KEYBOARD;
    inputs[5].ki.wVk = VK_MENU;
    inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(6, inputs, sizeof(INPUT));
    lastSentTime = now; // Обновляем время последней отправки
    std::cout << "Key combination sent. Cooldown activated.\n";
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
    DisplayAsciiArtFromCode(50);
    const DWORD threshold = 50000 * 1024; // Порог 50 МБ
    bool wasBelowThreshold = false; // Для гистерезиса

    while (true) {
        HANDLE processHandle = FindSecondLargestNvContainerProcessHandle();
        if (!processHandle) {
            std::cerr << "Process not found. Retrying in 30 seconds...\n";
            std::this_thread::sleep_for(std::chrono::seconds(30));
            continue;
        }

        bool isBelow = IsNvContainerBelowThreshold(processHandle, threshold);

        // Гистерезис: отправляем команду только если состояние изменилось с высокого на низкое
        if (isBelow && !wasBelowThreshold) {
            std::cout << "Memory below threshold. Attempting to enable Instant Replay...\n";
            SimulateKeyCombination();
        }
        wasBelowThreshold = isBelow;

        CloseHandle(processHandle);
        std::this_thread::sleep_for(std::chrono::seconds(10)); // Проверка каждые 10 секунд
    }

    return 0;
}
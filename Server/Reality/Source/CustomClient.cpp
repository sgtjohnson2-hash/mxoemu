#include "CustomClient.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

void DrawMatrixCodeRain() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
#else
    std::cout << "\033[1;32m";
#endif
    
    int width = 80;
    int height = 24;
    std::vector<int> drops(width, 0);

    for (int i = 0; i < 30; ++i) {
        for (int x = 0; x < width; ++x) {
            if (drops[x] == 0) {
                if (rand() % 100 < 5) drops[x] = 1;
            } else {
                drops[x]++;
                if (drops[x] > height) drops[x] = 0;
            }
            
            if (drops[x] > 0) {
                char c = 33 + (rand() % 94);
                std::cout << c;
            } else {
                std::cout << " ";
            }
        }
        std::cout << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[2J\033[H";
#endif
}

void CustomClient::Run(const std::string& username, const std::string& token) {
    DrawMatrixCodeRain();
    
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
#else
    std::cout << "\033[1;32m";
#endif

    std::cout << "========================================================\n";
    std::cout << "          ZION MAINFRAME SECURE CONNECTION              \n";
    std::cout << "========================================================\n\n";

    std::cout << "> Initializing connection to Auth Server [127.0.0.1:20000]...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    std::cout << "> Sending credentials for operator: " << username << "...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    if (!token.empty()) {
        std::cout << "> Received encrypted session token from Launcher.\n";
        std::cout << "> Token Hash: " << token << "\n";
    }
    
    std::cout << "> AUTHENTICATION SUCCESSFUL. Retrieving server list...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "> Connecting to Margin Server (Vector) [127.0.0.1:20001]...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    std::cout << "> CONSTRUCT LOADED.\n\n";
    std::cout << "Character Selection:\n";
    std::cout << "1. " << username << " [Redpill, Zion]\n";
    std::cout << "2. [Empty Slot]\n";
    std::cout << "3. [Empty Slot]\n\n";

    std::cout << "> Auto-selecting character 1...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::cout << "\n> Patching into Hardline...\n";
    for(int i=0; i<3; ++i) {
        std::cout << "." << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "\n";
    
#ifdef _WIN32
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
#else
    std::cout << "\033[1;33m";
#endif
    std::cout << "\n*** WARNING: YOU ARE NOW IN THE MATRIX ***\n\n";
#ifdef _WIN32
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
#else
    std::cout << "\033[1;32m";
#endif
    
    std::cout << "Welcome back, " << username << ".\n";
    std::cout << "[HEADLESS BOT MODE ACTIVE]\n";
    std::cout << "Your consciousness is now streaming into the construct.\n";
    
#ifdef _WIN32
    std::cout << "Press 'Q' to jack out...\n";
    while (true) {
        if (_kbhit()) {
            char key = _getch();
            if (key == 'q' || key == 'Q') {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    std::cout << "\033[0m\n";
#endif
    std::cout << "\n> Jacking out... Transferring to Zion Mainframe...\n";
}

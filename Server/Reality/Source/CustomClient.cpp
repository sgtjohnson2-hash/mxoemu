#include "CustomClient.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <windows.h>
#include <conio.h>

void DrawMatrixCodeRain() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    
    int width = 80;
    int height = 24;
    std::vector<int> drops(width, 0);

    for (int i = 0; i < 50; ++i) {
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
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Clear screen
    system("cls");
}

void CustomClient::Run(const std::string& username, const std::string& token) {
    DrawMatrixCodeRain();
    
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

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
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::cout << "\n*** WARNING: YOU ARE NOW IN THE MATRIX ***\n\n";
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    
    std::cout << "Welcome back, " << username << ".\n";
    std::cout << "[HEADLESS BOT MODE ACTIVE]\n";
    std::cout << "Your consciousness is now streaming into the construct.\n";
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
    
    std::cout << "\n> Jacking out... Transferring to Zion Mainframe...\n";
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

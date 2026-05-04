#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>

using namespace std;

CRITICAL_SECTION cs;

struct SharedData {
    int currentPair;
    int completedThreads;
};

struct ThreadParams {
    int pairNumber;
    int threadNumber;
    bool positive;
    int mode;
    HANDLE startEvent;
    HANDLE writeEvent;
    SharedData* sharedData;
};

void WriteNumberToFile(ofstream& file, ThreadParams* params, int value) {
    file << "Pair " << params->pairNumber
        << ", Thread " << params->threadNumber
        << ": " << value << endl;

    cout << "Pair " << params->pairNumber
        << ", Thread " << params->threadNumber
        << ": " << value << endl;
}

DWORD WINAPI ThreadFunction(LPVOID lpParam) {
    ThreadParams* params = (ThreadParams*)lpParam;

    WaitForSingleObject(params->startEvent, INFINITE);

    string fileName;

    if (params->mode == 1) {
        fileName = "result_without_sync.txt";
    }
    else if (params->mode == 2) {
        fileName = "result_event_sync.txt";
    }
    else {
        fileName = "result_critical_section.txt";
    }

    for (int i = 1; i <= 500; i++) {
        int value = params->positive ? i : -i;

        if (params->mode == 1) {
            ofstream file(fileName, ios::app);
            WriteNumberToFile(file, params, value);
            file.close();
        }

        else if (params->mode == 2) {
            WaitForSingleObject(params->writeEvent, INFINITE);

            ofstream file(fileName, ios::app);
            WriteNumberToFile(file, params, value);
            file.close();

            SetEvent(params->writeEvent);
        }

        else if (params->mode == 3) {
            EnterCriticalSection(&cs);

            ofstream file(fileName, ios::app);
            WriteNumberToFile(file, params, value);
            file.close();

            LeaveCriticalSection(&cs);
        }

        Sleep(5);
    }

    EnterCriticalSection(&cs);
    params->sharedData->completedThreads++;
    LeaveCriticalSection(&cs);

    return 0;
}

void RunMode(int mode) {
    cout << endl;
    cout << "========================================" << endl;

    if (mode == 1) {
        cout << "MODE 1: WITHOUT SYNCHRONIZATION" << endl;
        ofstream clearFile("result_without_sync.txt");
        clearFile.close();
    }
    else if (mode == 2) {
        cout << "MODE 2: SYNCHRONIZATION WITH EVENTS" << endl;
        ofstream clearFile("result_event_sync.txt");
        clearFile.close();
    }
    else {
        cout << "MODE 3: SYNCHRONIZATION WITH CRITICAL SECTION" << endl;
        ofstream clearFile("result_critical_section.txt");
        clearFile.close();
    }

    cout << "========================================" << endl;

    HANDLE hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedData),
        L"Local\\LabSharedMemory"
    );

    if (hMapFile == NULL) {
        cout << "CreateFileMapping error: " << GetLastError() << endl;
        return;
    }

    SharedData* sharedData = (SharedData*)MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(SharedData)
    );

    if (sharedData == NULL) {
        cout << "MapViewOfFile error: " << GetLastError() << endl;
        CloseHandle(hMapFile);
        return;
    }

    sharedData->currentPair = 0;
    sharedData->completedThreads = 0;

    const int PAIRS = 3;
    const int THREADS_PER_PAIR = 2;
    const int TOTAL_THREADS = PAIRS * THREADS_PER_PAIR;

    HANDLE threads[TOTAL_THREADS];
    HANDLE startEvents[PAIRS];

    HANDLE writeEvent = CreateEvent(
        NULL,
        FALSE,
        TRUE,
        NULL
    );

    ThreadParams params[TOTAL_THREADS];

    for (int i = 0; i < PAIRS; i++) {
        startEvents[i] = CreateEvent(
            NULL,
            TRUE,
            FALSE,
            NULL
        );
    }

    int index = 0;

    for (int pair = 1; pair <= PAIRS; pair++) {
        for (int t = 1; t <= THREADS_PER_PAIR; t++) {
            params[index].pairNumber = pair;
            params[index].threadNumber = t;
            params[index].positive = (t == 1);
            params[index].mode = mode;
            params[index].startEvent = startEvents[pair - 1];
            params[index].writeEvent = writeEvent;
            params[index].sharedData = sharedData;

            threads[index] = CreateThread(
                NULL,
                0,
                ThreadFunction,
                &params[index],
                CREATE_SUSPENDED,
                NULL
            );

            if (threads[index] == NULL) {
                cout << "CreateThread error: " << GetLastError() << endl;
                return;
            }

            if (t == 1) {
                SetThreadPriority(threads[index], THREAD_PRIORITY_ABOVE_NORMAL);
            }
            else {
                SetThreadPriority(threads[index], THREAD_PRIORITY_BELOW_NORMAL);
            }

            index++;
        }
    }

    cout << "All threads created in suspended state." << endl;

    for (int pair = 1; pair <= PAIRS; pair++) {
        cout << endl;
        cout << "Starting pair " << pair << endl;

        sharedData->currentPair = pair;

        int firstThreadIndex = (pair - 1) * 2;
        int secondThreadIndex = firstThreadIndex + 1;

        ResumeThread(threads[firstThreadIndex]);
        ResumeThread(threads[secondThreadIndex]);

        SetEvent(startEvents[pair - 1]);

        WaitForSingleObject(threads[firstThreadIndex], INFINITE);
        WaitForSingleObject(threads[secondThreadIndex], INFINITE);

        cout << "Pair " << pair << " finished." << endl;
    }

    cout << "Completed threads: " << sharedData->completedThreads << endl;

    for (int i = 0; i < TOTAL_THREADS; i++) {
        CloseHandle(threads[i]);
    }

    for (int i = 0; i < PAIRS; i++) {
        CloseHandle(startEvents[i]);
    }

    CloseHandle(writeEvent);

    UnmapViewOfFile(sharedData);
    CloseHandle(hMapFile);
}

int main() {
    InitializeCriticalSection(&cs);

    RunMode(1);
    RunMode(2);
    RunMode(3);

    DeleteCriticalSection(&cs);

    cout << endl;
    cout << "All modes finished successfully." << endl;

    return 0;
}
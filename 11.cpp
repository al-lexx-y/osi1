#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

char g_currentDir[MAX_PATH] = "No current directory now";

string DriveTypeToString(int type){
    switch(type){
        case 0: return ("Unknown drive");
        case 1: return ("No root directory");
        case 2: return ("Removable drive");
        case 3: return ("Fixed disk");
        case 4: return ("Network drive");
        case 5: return ("CD/DVD/Blu-ray");
        case 6: return ("RAM disk");
        default: return ("???");
    }
}

void ShowDriveDetails(string path){    
    int driveType = GetDriveType(path.c_str());
    cout <<"\n1. Drive type: " << DriveTypeToString(driveType) << endl;

    TCHAR volName[MAX_PATH] = {0};
    DWORD serialNum = 0; 
    DWORD maxCompLen = 0;
    DWORD fsFlags = 0;
    TCHAR fsName[MAX_PATH] = {0};
   
    bool success = GetVolumeInformation(
        path.c_str(),
        volName, 
        MAX_PATH, 
        &serialNum, 
        &maxCompLen, 
        &fsFlags, 
        fsName, 
        MAX_PATH);

    if(!success){
        DWORD err = GetLastError();
        wcerr<<"ERROR: "<<err;
    }
    else{
        cout<<"\n2. Volume information:"<<endl;
        cout<<"2.1. Path: "<< path<<endl;
        cout<<"2.2. Volume name: ";
        #ifdef UNICODE
            wcout<<volName<<endl;
        #else
            wcout<<volName<<endl;
        #endif
        cout << "2.3. Serial number: 0x" << hex << uppercase << serialNum << dec << endl;
        cout << "2.4. Max component length: " << maxCompLen << endl;
        cout << "2.5. File system flags: 0x" << hex << uppercase << fsFlags << dec << endl;
        cout << "2.6. File system: ";
        
        #ifdef UNICODE
            wcout << fsName << endl;
        #else
            cout << fsName << endl;
        #endif
        
        cout << "2.7. Features:" << endl;
        if (fsFlags & FILE_CASE_SENSITIVE_SEARCH)
            cout << "  - Case-sensitive search" << endl;
        if (fsFlags & FILE_CASE_PRESERVED_NAMES)
            cout << "  - Preserves case" << endl;
        if (fsFlags & FILE_UNICODE_ON_DISK)
            cout << "  - Unicode support" << endl;
        if (fsFlags & FILE_PERSISTENT_ACLS)
            cout << "  - ACLs preserved" << endl;
        if (fsFlags & FILE_FILE_COMPRESSION)
            cout << "  - Compression support" << endl;
        if (fsFlags & FILE_VOLUME_QUOTAS)
            cout << "  - Quotas support" << endl;
        if (fsFlags & FILE_SUPPORTS_SPARSE_FILES)
            cout << "  - Sparse files" << endl;
        if (fsFlags & FILE_SUPPORTS_REPARSE_POINTS)
            cout << "  - Reparse points" << endl;
        if (fsFlags & FILE_VOLUME_IS_COMPRESSED)
            cout << "  - Volume compressed" << endl;
        if (fsFlags & FILE_READ_ONLY_VOLUME)
            cout << "  - Read-only volume" << endl;
    }

    ULARGE_INTEGER freeAvail;
    ULARGE_INTEGER totalBytes;
    ULARGE_INTEGER freeBytes;
    
    BOOL spaceOk = GetDiskFreeSpaceExA(path.c_str(), &freeAvail, &totalBytes, &freeBytes);
    
    if (!spaceOk) {
        cout << "Error getting space: " << GetLastError() << endl;
    } else {
        cout << "\n3. Disk space:" << endl;
        cout << "3.1. Total: " << totalBytes.QuadPart / (1024 * 1024) << " MB" << endl;
        cout << "3.2. Free: " << freeBytes.QuadPart / (1024 * 1024) << " MB" << endl;
        cout << "3.3. Available: " << freeAvail.QuadPart / (1024 * 1024) << " MB" << endl;
    }
}

void MakeDirectory(){
    string fullPath;
    cout <<"Enter full path for new directory (e.g., C:\\NewFolder or just NewFolder for current): ";
    cin.ignore(10000, '\n');
    getline(cin, fullPath);
    
    if (fullPath.empty()) {
        cout << "No path entered. Operation cancelled." << endl;
        return;
    }
    
    if(CreateDirectory(fullPath.c_str(), NULL)){
        cout<<"Directory "<< fullPath << " created successfully!" << endl;
    }else{
        DWORD err = GetLastError();
        cout << "Error: " << err << endl;
        if (err == ERROR_ALREADY_EXISTS) {
            cout << "Directory already exists!" << endl;
        } else if (err == ERROR_PATH_NOT_FOUND) {
            cout << "Path not found!" << endl;
        }
    }
}

void DeleteDirectory(){
    string pathName;
    cout <<"Enter directory path to remove: ";
    cin >> pathName;
    if(RemoveDirectory(pathName.c_str())){
        cout<<"Directory "<< pathName << " removed successfully!" << endl;
    }else{
        cout << "Error: " << GetLastError() << endl;
    }
}

void ListCurrentDirectory() {
    string searchPath = string(g_currentDir) + "\\*";
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        cout << "Error reading directory: " << GetLastError() << endl;
        return;
    }
    
    cout << "\n=== Contents of: " << g_currentDir << " ===\n" << endl;
    cout << "DIRECTORIES:" << endl;
    cout << "------------" << endl;
    
    vector<string> files;
    vector<string> dirs;
    
    do {
        string name = findData.cFileName;
        if (name != "." && name != "..") {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                dirs.push_back(name);
            } else {
                files.push_back(name);
            }
        }
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
    
    for (const auto& dir : dirs) {
        cout << "  [DIR]  " << dir << endl;
    }
    
    cout << "\nFILES:" << endl;
    cout << "------" << endl;
    
    for (const auto& file : files) {
        string fullPath = string(g_currentDir) + "\\" + file;
        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        
        if (GetFileAttributesEx(fullPath.c_str(), GetFileExInfoStandard, &fileInfo)) {
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = fileInfo.nFileSizeLow;
            fileSize.HighPart = fileInfo.nFileSizeHigh;
            
            cout << "  [FILE] " << file;
            if (fileSize.QuadPart < 1024) {
                cout << " (" << fileSize.QuadPart << " bytes)" << endl;
            } else if (fileSize.QuadPart < 1024 * 1024) {
                cout << " (" << fileSize.QuadPart / 1024 << " KB)" << endl;
            } else {
                cout << " (" << fileSize.QuadPart / (1024 * 1024) << " MB)" << endl;
            }
        } else {
            cout << "  [FILE] " << file << endl;
        }
    }
    
    cout << "\nTotal: " << dirs.size() << " directories, " << files.size() << " files" << endl;
    cout << "========================================\n" << endl;
}

void MakeFile(){
    string dirPath, fileName;
    int answer;
    DWORD accessMode = 0;
    DWORD shareMode = 0;  
    DWORD createDisp = CREATE_NEW; 
    DWORD fileFlags = 0;
    int userChoice;
    bool selectionDone = false;

    cout << "Enter directory path (press Enter for current): ";
    cin.ignore(10000, '\n');
    getline(cin, dirPath);
    if (dirPath.empty()) {
        dirPath = g_currentDir;
        cout << "Using current: " << dirPath << endl;
    }
    cout << "\nEnter file name: ";
    cin >> fileName;
    string fullFilePath = dirPath + "\\" + fileName;
   
    cout << "\nSelect access mode:\n";
    cout << "1. Read only\n";
    cout << "2. Write only\n";
    cout << "3. Read and write\n";
    cout << "Choice: ";
    cin >> answer;

    while(answer < 1 || answer > 3){
        cout<<"Invalid. Try again (1-3): ";
        cin >> answer;
    }
    
    switch(answer){
        case 1:
            accessMode = GENERIC_READ;
            cout << "Selected: Read only\n";
            break;
        case 2:
            accessMode = GENERIC_WRITE;
            cout << "Selected: Write only\n";
            break;
        case 3:
            accessMode = GENERIC_READ | GENERIC_WRITE;
            cout << "Selected: Read and write\n";
            break;
    }
    
    cout << "\nSelect sharing mode:\n";
    cout << "0. No sharing\n";
    cout << "1. Read sharing\n";
    cout << "2. Write sharing\n";
    cout << "3. Delete sharing\n";
    cout << "4. All sharing\n";
    cout << "Choice: ";
    cin >> answer;
    
    while(answer < 0 || answer > 4){
        cout<<"Invalid. Try again (0-4): ";
        cin >> answer;
    }
    
    switch(answer) {
        case 0:
            shareMode = 0;
            cout << "Selected: Exclusive\n";
            break;
        case 1:
            shareMode = FILE_SHARE_READ;
            cout << "Selected: Read sharing\n";
            break;
        case 2:
            shareMode = FILE_SHARE_WRITE;
            cout << "Selected: Write sharing\n";
            break;
        case 3:
            shareMode = FILE_SHARE_DELETE;
            cout << "Selected: Delete sharing\n";
            break;
        case 4:
            shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
            cout << "Selected: All sharing\n";
            break;
    }
    
    cout << "\nIf file exists:\n";
    cout << "1. Fail (CREATE_NEW)\n";
    cout << "2. Overwrite (CREATE_ALWAYS)\n";
    cout << "Choice: ";
    cin >> answer;
    
    while(answer < 1 || answer > 2){
        cout << "Invalid (1-2): ";
        cin >> answer;
    }
    
    switch(answer) {
        case 1:
            createDisp = CREATE_NEW;
            cout << "Selected: Fail if exists\n";
            break;
        case 2:
            createDisp = CREATE_ALWAYS;
            cout << "Selected: Overwrite\n";
            break;
    }

    fileFlags = 0;
    selectionDone = false;
    
    cout << "\nSelect attributes/flags (0 when done):\n";
    cout << "1. NORMAL\n";
    cout << "2. READONLY\n";    
    cout << "3. HIDDEN\n";
    cout << "4. SYSTEM\n";
    cout << "5. TEMPORARY\n";
    cout << "6. ARCHIVE\n";
    cout << "7. DELETE_ON_CLOSE\n";
    cout << "8. RANDOM_ACCESS\n";
    cout << "9. SEQUENTIAL_SCAN\n";
    cout << "10. OVERLAPPED\n";
    cout << "11. NO_BUFFERING\n";
    cout << "0. Done\n";

    while (!selectionDone) {
        cout << "Add flag (0-11): ";
        cin >> userChoice;

        if (userChoice < 0 || userChoice > 11) {
            cout << "Invalid! Enter 0-11\n";
            continue;
        }
    
        switch(userChoice) {
            case 0:
                selectionDone = true;
                cout << "Selection complete.\n";
                break;
            case 1:
                fileFlags |= FILE_ATTRIBUTE_NORMAL;
                cout << "  Added: NORMAL\n";
                break;
            case 2:
                fileFlags |= FILE_ATTRIBUTE_READONLY;
                cout << "  Added: READONLY\n";
                break;
            case 3:
                fileFlags |= FILE_ATTRIBUTE_HIDDEN;
                cout << "  Added: HIDDEN\n";
                break;
            case 4:
                fileFlags |= FILE_ATTRIBUTE_SYSTEM;
                cout << "  Added: SYSTEM\n";
                break;
            case 5:
                fileFlags |= FILE_ATTRIBUTE_TEMPORARY;
                cout << "  Added: TEMPORARY\n";
                break;
            case 6:
                fileFlags |= FILE_ATTRIBUTE_ARCHIVE;
                cout << "  Added: ARCHIVE\n";
                break;
            case 7:
                fileFlags |= FILE_FLAG_DELETE_ON_CLOSE;
                cout << "  Added: DELETE_ON_CLOSE\n";
                break;
            case 8:
                fileFlags |= FILE_FLAG_RANDOM_ACCESS;
                cout << "  Added: RANDOM_ACCESS\n";
                break;
            case 9:
                fileFlags |= FILE_FLAG_SEQUENTIAL_SCAN;
                cout << "  Added: SEQUENTIAL_SCAN\n";
                break;
            case 10:
                fileFlags |= FILE_FLAG_OVERLAPPED;
                cout << "  Added: OVERLAPPED\n";
                break;
            case 11:
                fileFlags |= FILE_FLAG_NO_BUFFERING;
                cout << "  Added: NO_BUFFERING\n";
                break;
        }
    }

    if (fileFlags == 0) {
        fileFlags = FILE_ATTRIBUTE_NORMAL;
        cout << "\nDefault: NORMAL selected\n";
    } else {
        cout << "\nFinal flags: " << fileFlags << endl;
    }
    
    cout << "\n--- Creating file ---\n";
    cout << "Path: " << fullFilePath << endl;
    
    HANDLE hFile = CreateFileA(
        fullFilePath.c_str(),
        accessMode,
        shareMode,
        NULL,
        createDisp,
        fileFlags,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        cout << "\nERROR: Failed to create file!\n";
        cout << "Error code: " << err << endl;
        
        switch(err) {
            case ERROR_FILE_EXISTS:
                cout << "File already exists! Use CREATE_ALWAYS to overwrite.\n";
                break;
            case ERROR_PATH_NOT_FOUND:
                cout << "Path not found.\n";
                break;
            case ERROR_ACCESS_DENIED:
                cout << "Access denied.\n";
                break;
            default:
                cout << "Unknown error\n";
        }
    } else {
        cout << "\nFile created successfully!\n";
        cout << "Handle: " << hFile << endl;
        CloseHandle(hFile);
        cout << "Handle closed.\n";
    }
}

void OpenExistingFile() {
    string dirPath, fileName; 
    cout << "Enter directory path (press Enter for current): ";
    cin.ignore(10000, '\n');
    getline(cin, dirPath);
    if (dirPath.empty()) {
        dirPath = g_currentDir;
        cout << "Using current: " << dirPath << endl;
    }
    cout << "\nEnter file name: ";
    cin >> fileName;
    string fullPath = dirPath + "\\" + fileName;
    
    HANDLE hFile = CreateFileA(
        fullPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile != INVALID_HANDLE_VALUE) {
        cout << "File opened successfully!" << endl;
        CloseHandle(hFile);
    } else {
        cout << "Failed to open. Error: " << GetLastError() << endl;
    }
}

void ChangeWorkingDir() {
    string dirPath;
    cout << "Enter directory path: ";
    cin >> dirPath;

    if (SetCurrentDirectoryA(dirPath.c_str())) {
        cout << "Changed to: " << dirPath << endl;
        GetCurrentDirectoryA(MAX_PATH, g_currentDir);
        cout << "Now in: " << g_currentDir << endl;
    } else {
        DWORD err = GetLastError();
        cout << "Error changing directory!" << endl;
        switch(err) {
            case ERROR_PATH_NOT_FOUND:
                cout << "Path not found" << endl;
                break;
            case ERROR_ACCESS_DENIED:
                cout << "Access denied" << endl;
                break;
            default:
                cout << "Error code: " << err << endl;
        }
    }
}

void DuplicateFile() {
    string srcPath, dstPath, fName;
    
    cout << "Enter source path (press Enter for current): ";
    cin.ignore(10000, '\n');
    getline(cin, srcPath);
    if (srcPath.empty()) {
        srcPath = g_currentDir;
        cout << "Using current: " << srcPath << endl;
    }
    
    cout << "Enter file name: ";
    getline(cin, fName);
    
    cout << "Enter destination path (press Enter for current): ";
    getline(cin, dstPath);
    if (dstPath.empty()) {
        dstPath = g_currentDir;
        cout << "Using current: " << dstPath << endl;
    }
    
    string srcFull = srcPath + "\\" + fName;
    string dstFull = dstPath + "\\" + fName;
    
    DWORD srcAttr = GetFileAttributesA(srcFull.c_str());
    if (srcAttr == INVALID_FILE_ATTRIBUTES) {
        cout << "Error: Source file not found!\n";
        return;
    }
    
    bool destExists = (GetFileAttributesA(dstFull.c_str()) != INVALID_FILE_ATTRIBUTES);
    
    if (destExists) {
        cout << "\nFile already exists at destination!\n";
        cout << "1. Overwrite\n";
        cout << "2. Cancel\n";
        cout << "Choice: ";
        
        int opt;
        cin >> opt;
        
        if (opt != 1) {
            cout << "Cancelled.\n";
            return;
        }
    }
    
    if (CopyFileA(srcFull.c_str(), dstFull.c_str(), !destExists)) {
        cout << "\nFile copied!\n";
        cout << "From: " << srcFull << endl;
        cout << "To: " << dstFull << endl;
    } else {
        DWORD err = GetLastError();
        cout << "\nCopy failed. Error: " << err << endl;
    }
}

void RelocateFile() {
    string srcPath, dstPath, fName;
    int moveOpt;
    
    cout << "Enter source path (press Enter for current): ";
    cin.ignore(10000, '\n');
    getline(cin, srcPath);
    if (srcPath.empty()) {
        srcPath = g_currentDir;
        cout << "Using current: " << srcPath << endl;
    }
    
    cout << "Enter file name: ";
    getline(cin, fName);
    
    cout << "Enter destination path (press Enter for current): ";
    getline(cin, dstPath);
    if (dstPath.empty()) {
        dstPath = g_currentDir;
        cout << "Using current: " << dstPath << endl;
    }
    
    cout << "\nMove options:\n";
    cout << "1. Simple move (fail if exists)\n";
    cout << "2. Move with overwrite\n";
    cout << "3. Move after reboot\n";
    cout << "Choice: ";
    cin >> moveOpt;
    
    string srcFull = srcPath + "\\" + fName;
    string dstFull = dstPath + "\\" + fName;
    
    DWORD srcAttr = GetFileAttributesA(srcFull.c_str());
    if (srcAttr == INVALID_FILE_ATTRIBUTES) {
        cout << "Error: Source file not found!\n";
        return;
    }
    
    bool destExists = (GetFileAttributesA(dstFull.c_str()) != INVALID_FILE_ATTRIBUTES);
    
    if (destExists && moveOpt == 1) {
        cout << "\nDestination exists! Use option 2 to overwrite.\n";
        return;
    }
    
    DWORD moveFlags = 0;
    switch(moveOpt) {
        case 1:
            moveFlags = MOVEFILE_COPY_ALLOWED;
            break;
        case 2:
            moveFlags = MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING;
            break;
        case 3:
            moveFlags = MOVEFILE_DELAY_UNTIL_REBOOT;
            cout << "\nWill move after reboot!\n";
            break;
        default:
            moveFlags = MOVEFILE_COPY_ALLOWED;
    }
    
    if (MoveFileExA(srcFull.c_str(), dstFull.c_str(), moveFlags)) {
        cout << "\nFile moved!\n";
        cout << "From: " << srcFull << endl;
        cout << "To: " << dstFull << endl;
    } else {
        DWORD err = GetLastError();
        cout << "\nMove failed. Error: " << err << endl;
    }
}

void DisplayFileAttributes() {
    string dirPath, fName;
    
    cout << "Enter directory path (press Enter for current): ";
    cin.ignore(10000, '\n');
    getline(cin, dirPath);
    if (dirPath.empty()) {
        dirPath = g_currentDir;
        cout << "Using current: " << dirPath << endl;
    }
    
    cout << "Enter file name: ";
    getline(cin, fName);
    
    string fullPath = dirPath + "\\" + fName;
    
    DWORD attrs = GetFileAttributesA(fullPath.c_str());
    
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        cout << "Error: Cannot get attributes. File not found.\n";
        return;
    }
    
    cout << "\n=== File Attributes ===\n";
    cout << "File: " << fullPath << endl;
    cout << "\nAttributes:\n";
    cout << "  Normal: " << ((attrs & FILE_ATTRIBUTE_NORMAL) ? "Yes" : "No") << endl;
    cout << "  Read-only: " << ((attrs & FILE_ATTRIBUTE_READONLY) ? "Yes" : "No") << endl;
    cout << "  Hidden: " << ((attrs & FILE_ATTRIBUTE_HIDDEN) ? "Yes" : "No") << endl;
    cout << "  System: " << ((attrs & FILE_ATTRIBUTE_SYSTEM) ? "Yes" : "No") << endl;
    cout << "  Archive: " << ((attrs & FILE_ATTRIBUTE_ARCHIVE) ? "Yes" : "No") << endl;
    cout << "  Temporary: " << ((attrs & FILE_ATTRIBUTE_TEMPORARY) ? "Yes" : "No") << endl;
    cout << "  Compressed: " << ((attrs & FILE_ATTRIBUTE_COMPRESSED) ? "Yes" : "No") << endl;
    cout << "  Encrypted: " << ((attrs & FILE_ATTRIBUTE_ENCRYPTED) ? "Yes" : "No") << endl;
    
    HANDLE hFile = CreateFileA(
        fullPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info;
        if (GetFileInformationByHandle(hFile, &info)) {
            cout << "\nDetailed info:\n";
            cout << "  Size: " << info.nFileSizeLow << " bytes\n";
            cout << "  Index: " << info.nFileIndexLow << endl;
            cout << "  Links: " << info.nNumberOfLinks << endl;
            
            SYSTEMTIME st;
            
            FileTimeToSystemTime(&info.ftCreationTime, &st);
            cout << "  Created: " << st.wDay << "." << st.wMonth << "." << st.wYear 
                 << " " << st.wHour << ":" << st.wMinute << endl;
            
            FileTimeToSystemTime(&info.ftLastAccessTime, &st);
            cout << "  Accessed: " << st.wDay << "." << st.wMonth << "." << st.wYear 
                 << " " << st.wHour << ":" << st.wMinute << endl;
            
            FileTimeToSystemTime(&info.ftLastWriteTime, &st);
            cout << "  Modified: " << st.wDay << "." << st.wMonth << "." << st.wYear 
                 << " " << st.wHour << ":" << st.wMinute << endl;
        }
        CloseHandle(hFile);
    }
}

void ModifyFileAttributes() {
    string dirPath, fName;
    int option;
    DWORD newAttrs = 0;
    
    cout << "Enter directory path (press Enter for current): ";
    cin.ignore(10000, '\n');
    getline(cin, dirPath);
    if (dirPath.empty()) {
        dirPath = g_currentDir;
        cout << "Using current: " << dirPath << endl;
    }
    
    cout << "Enter file name: ";
    getline(cin, fName);
    
    string fullPath = dirPath + "\\" + fName;
    
    DWORD currentAttrs = GetFileAttributesA(fullPath.c_str());
    if (currentAttrs == INVALID_FILE_ATTRIBUTES) {
        cout << "Error: File not found.\n";
        return;
    }
    
    cout << "\nCurrent attributes:\n";
    cout << "  Read-only: " << ((currentAttrs & FILE_ATTRIBUTE_READONLY) ? "Yes" : "No") << endl;
    cout << "  Hidden: " << ((currentAttrs & FILE_ATTRIBUTE_HIDDEN) ? "Yes" : "No") << endl;
    cout << "  System: " << ((currentAttrs & FILE_ATTRIBUTE_SYSTEM) ? "Yes" : "No") << endl;
    cout << "  Archive: " << ((currentAttrs & FILE_ATTRIBUTE_ARCHIVE) ? "Yes" : "No") << endl;
    
    cout << "\nSelect new attributes:\n";
    cout << "1. Read-only\n";
    cout << "2. Hidden\n";
    cout << "3. System\n";
    cout << "4. Archive\n";
    cout << "5. Normal (clear all)\n";
    cout << "0. Cancel\n";
    cout << "Choice: ";
    cin >> option;
    
    if (option == 0) return;
    
    if (option == 5) {
        newAttrs = FILE_ATTRIBUTE_NORMAL;
    } else {
        newAttrs = currentAttrs;
        
        if (option == 1) newAttrs ^= FILE_ATTRIBUTE_READONLY;
        else if (option == 2) newAttrs ^= FILE_ATTRIBUTE_HIDDEN;
        else if (option == 3) newAttrs ^= FILE_ATTRIBUTE_SYSTEM;
        else if (option == 4) newAttrs ^= FILE_ATTRIBUTE_ARCHIVE;
    }
    
    if (SetFileAttributesA(fullPath.c_str(), newAttrs)) {
        cout << "\nAttributes changed!\n";
    } else {
        DWORD err = GetLastError();
        cout << "\nFailed to change. Error: " << err << endl;
    }
}

void AdjustFileTime() {
    string dirPath, fName;
    int option;
    
    cout << "Enter directory path (press Enter for current): ";
    cin.ignore(10000, '\n');
    getline(cin, dirPath);
    if (dirPath.empty()) {
        dirPath = g_currentDir;
        cout << "Using current: " << dirPath << endl;
    }
    
    cout << "Enter file name: ";
    getline(cin, fName);
    
    string fullPath = dirPath + "\\" + fName;
    
    HANDLE hFile = CreateFileA(
        fullPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        cout << "Error: Cannot open file.\n";
        return;
    }
    
    cout << "\nWhich time to change?\n";
    cout << "1. Creation time\n";
    cout << "2. Last access time\n";
    cout << "3. Last write time\n";
    cout << "4. All times\n";
    cout << "Choice: ";
    cin >> option;
    
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    
    BOOL result = FALSE;
    switch(option) {
        case 1:
            result = SetFileTime(hFile, &ft, NULL, NULL);
            break;
        case 2:
            result = SetFileTime(hFile, NULL, &ft, NULL);
            break;
        case 3:
            result = SetFileTime(hFile, NULL, NULL, &ft);
            break;
        case 4:
            result = SetFileTime(hFile, &ft, &ft, &ft);
            break;
    }
    
    if (result) {
        cout << "\nTime updated!\n";
    } else {
        DWORD err = GetLastError();
        cout << "\nFailed. Error: " << err << endl;
    }
    
    CloseHandle(hFile);
}

int main() {
    bool exitApp = false;
    
    GetCurrentDirectoryA(MAX_PATH, g_currentDir);
    cout << "Starting in: " << g_currentDir << endl;
    
    while (!exitApp) {
        cout << "\n===== MAIN MENU =====" << endl;
        cout << "1. List all drives" << endl;
        cout << "2. Show drive information" << endl;
        cout << "3. Create directory" << endl;
        cout << "4. Remove directory" << endl;
        cout << "5. Create file" << endl;
        cout << "6. Open file" << endl;
        cout << "7. Copy file" << endl;
        cout << "8. Move file" << endl;
        cout << "9. Show file attributes" << endl;
        cout << "10. Change file attributes" << endl;
        cout << "11. Change file timestamps" << endl;
        cout << "12. Change working directory" << endl;
        cout << "13. Show current directory contents" << endl;
        cout << "14. Exit" << endl;
        cout << "Choice: ";
        
        int menuChoice;
        cin >> menuChoice;
        
        switch (menuChoice) {
            case 1: {
                char drives[256];
                DWORD result = GetLogicalDriveStringsA(256, drives);
                
                if (result == 0) {
                    cout << "Error getting drives" << endl;
                    break;
                }
                
                cout << "\nAvailable drives:" << endl;
                char* ptr = drives;
                while (*ptr) {
                    cout << "  " << ptr << endl;
                    ptr += strlen(ptr) + 1;
                }
                break;
            }
            
            case 2: {
                char driveLetter;
                cout << "Enter drive letter (C, D, etc.): ";
                cin >> driveLetter;
                
                while(!((driveLetter >= 'A' && driveLetter <= 'Z') || 
                       (driveLetter >= 'a' && driveLetter <= 'z'))){
                    cout << "Invalid! Enter A-Z: ";
                    cin >> driveLetter;
                }
                
                string drivePath = string(1, driveLetter) + ":\\";
                ShowDriveDetails(drivePath);
                break;
            }
            
            case 3:
                MakeDirectory();
                break;
                
            case 4:
                DeleteDirectory();
                break;
                
            case 5:
                MakeFile();
                break;
                
            case 6:
                OpenExistingFile();
                break;
                
            case 7:
                DuplicateFile();
                break;
                
            case 8:
                RelocateFile();
                break;
                
            case 9:
                DisplayFileAttributes();
                break;
                
            case 10:
                ModifyFileAttributes();
                break;
                
            case 11:
                AdjustFileTime();
                break;
                
            case 12:
                ChangeWorkingDir();
                break;

            case 13:
                ListCurrentDirectory();
                break;
                
            case 14:
                exitApp = true;
                cout << "Goodbye!" << endl;
                break;
                
            default:
                cout << "Invalid choice." << endl;
        }
    }
    
    return 0;
}

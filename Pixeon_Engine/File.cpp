#include "File.h"
#include <Windows.h>

// Explorer�Ŏw��p�X���J��
void File::OpenExplorer(const std::string& path){
	ShellExecuteA(
		NULL,           
		"open",         
		"explorer.exe", 
		path.c_str(),   
		NULL,           
		SW_SHOWNORMAL   
	);
}

// ���s�t�@�C���̃p�X���擾
std::string File::GetExePath(){
	char path[MAX_PATH];
	DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
	if (length == 0 || length == MAX_PATH) return "";
	return std::string(path, length);
}

// ���s�t�@�C�������������p�X���擾
std::string File::RemoveExeFromPath(const std::string& exePath){
	size_t pos = exePath.find_last_of("\\/");
	if (pos != std::string::npos)return exePath.substr(0, pos);
	
	return exePath;
}

// �A�Z�b�g�p�b�J�[���Ăяo��
bool File::CallAssetPacker(const std::string& toolPath, const std::string& assetDir, const std::string& outputPak){
	std::string cmd = "\"" + toolPath + "\" \"" + assetDir + "\" \"" + outputPak + "\"";
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	BOOL result = CreateProcessA(
		nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

	if (!result) return false;

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (exitCode == 0);
}

// �A�[�J�C�u�c�[�������s
bool File::RunArchiveTool(const std::string& toolExePath, const std::string& assetDir, const std::string& archivePath){
	std::string cmd = "\"" + toolExePath + "\" \"" + assetDir + "\" \"" + archivePath + "\"";

	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	BOOL result = CreateProcessA(
		nullptr,            // lpApplicationName
		(LPSTR)cmd.c_str(), // lpCommandLine
		nullptr,            // lpProcessAttributes
		nullptr,            // lpThreadAttributes
		FALSE,              // bInheritHandles
		CREATE_NO_WINDOW,   // dwCreationFlags
		nullptr,            // lpEnvironment
		nullptr,            // lpCurrentDirectory
		&si,                // lpStartupInfo
		&pi                 // lpProcessInformation
	);

	if (!result) {
		return false;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (exitCode == 0);
}



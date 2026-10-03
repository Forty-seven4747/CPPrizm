#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <string>
#include <vector>
#include <cwchar>
#include <cstdio>

#define IDC_EDIT_SRC     1001
#define IDC_BTN_BROWSE   1002
#define IDC_BTN_BUILD    1003
#define IDC_BTN_OPENDIR  1004
#define IDC_EDIT_LOG     1005
#define IDC_STATIC_STATUS 1006
#define IDC_BTN_OPENSRC  1007
#define IDC_RADIO_FONT1  1008
#define IDC_RADIO_FONT2  1009
#define IDC_RADIO_FONT3  1010
#define IDC_RADIO_BGW    1011
#define IDC_RADIO_BGB    1012
#define IDC_BTN_BROWSEDIR 1013
#define IDC_EDIT_VER    1014

static HINSTANCE g_hInst = nullptr;
static HWND  g_hWnd = nullptr;
static HWND  g_hEditSrc = nullptr;
static HWND  g_hEditVer = nullptr;
static HWND  g_hEditLog = nullptr;
static HWND  g_hStatus = nullptr;
static HFONT g_hFontUI = nullptr;
static HFONT g_hFontMono = nullptr;
static HBRUSH g_hBrushLog = nullptr;
static std::wstring g_root;
static std::wstring g_origPath;
static std::wstring g_logBuf;
static bool g_headless = false;

/* Console appearance, baked into the .g3a at build time. */
static int g_conFont = 2;
static int g_conBg = 0;

/* Version string written into the .g3a header, shown in the calculator main
   menu.  Baked in at build time like the console appearance. */
static std::wstring g_g3aVersion = L"1.00";

/* Set by the option parser when a switch has a bad or missing value, so a typo
   fails the build instead of quietly falling back to the default. */
static std::wstring g_optError;

static const wchar_t* FontDesc(int f) {
	switch (f) {
	case 1:  return L"big font, PrintXY 24 px, 21 cols x 7 rows";
	case 3:  return L"small font, PrintMiniMini 10 px, 54 cols x 16 rows";
	default: return L"medium font, PrintMini 18 px, 32 cols x 9 rows";
	}
}

static const wchar_t* BgDesc(int b) {
	return b ? L"black paper" : L"white paper";
}

/* Version string for the .g3a header.  Restricted to ASCII letters, digits,
   dot, dash and underscore, 1..16 characters, so it can never break the
   mkg3a command line no matter what was typed. */
static bool ParseVersionOpt(const std::wstring& v, std::wstring& out) {
	if (v.empty() || v.size() > 16) return false;
	for (wchar_t c : v) {
		if (!((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'Z') ||
		      (c >= L'a' && c <= L'z') || c == L'.' || c == L'-' || c == L'_'))
			return false;
	}
	out = v;
	return true;
}

static std::wstring Join(const std::wstring& a, const std::wstring& b) {
	if (a.empty()) return b;
	wchar_t last = a[a.size() - 1];
	if (last == L'\\' || last == L'/') return a + b;
	return a + L"\\" + b;
}

static bool PathExists(const std::wstring& p) {
	DWORD a = GetFileAttributesW(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES;
}

static bool IsDir(const std::wstring& p) {
	DWORD a = GetFileAttributesW(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool CreateDirDeep(const std::wstring& path) {
	if (path.empty()) return false;
	if (IsDir(path)) return true;
	size_t pos = path.find_last_of(L"\\/");
	if (pos != std::wstring::npos && pos > 0) {
		std::wstring parent = path.substr(0, pos);

		bool isDriveRoot = (parent.size() == 2 && parent[1] == L':');
		if (!isDriveRoot && !CreateDirDeep(parent)) return false;
	}
	if (CreateDirectoryW(path.c_str(), NULL)) return true;
	return IsDir(path);
}

static bool DeleteDirDeep(const std::wstring& path) {
	if (!IsDir(path)) return true;
	std::wstring pattern = Join(path, L"*");
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			std::wstring nm = fd.cFileName;
			if (nm == L"." || nm == L"..") continue;
			std::wstring full = Join(path, nm);
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				DeleteDirDeep(full);
			} else {
				SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
				DeleteFileW(full.c_str());
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}
	return RemoveDirectoryW(path.c_str()) != 0;
}

static bool CopyDirDeep(const std::wstring& src, const std::wstring& dst) {
	if (!IsDir(src)) return false;
	if (!CreateDirDeep(dst)) return false;
	std::wstring pattern = Join(src, L"*");
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return true;
	do {
		std::wstring nm = fd.cFileName;
		if (nm == L"." || nm == L"..") continue;
		std::wstring s = Join(src, nm), d = Join(dst, nm);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) CopyDirDeep(s, d);
		else CopyFileW(s.c_str(), d.c_str(), FALSE);
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	return true;
}

static bool ReadAllBytes(const std::wstring& path, std::string& out) {
	out.clear();
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;
	LARGE_INTEGER sz;
	if (!GetFileSizeEx(h, &sz)) { CloseHandle(h); return false; }
	out.resize((size_t)sz.QuadPart);
	DWORD got = 0, total = 0;
	while (total < out.size()) {
		if (!ReadFile(h, &out[total], (DWORD)(out.size() - total), &got, NULL) || got == 0) break;
		total += got;
	}
	out.resize(total);
	CloseHandle(h);
	return true;
}

static bool WriteAllBytes(const std::wstring& path, const std::string& data) {
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
	                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0, total = 0;
	while (total < data.size()) {
		if (!WriteFile(h, data.data() + total, (DWORD)(data.size() - total), &written, NULL) || written == 0) break;
		total += written;
	}
	CloseHandle(h);
	return total == data.size();
}

static std::wstring BaseNameNoExt(const std::wstring& path) {
	size_t slash = path.find_last_of(L"\\/");
	std::wstring nm = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
	size_t dot = nm.find_last_of(L'.');
	if (dot != std::wstring::npos && dot > 0) nm = nm.substr(0, dot);
	return nm;
}

static std::wstring SanitizeName(const std::wstring& in) {
	std::wstring out;
	bool dropped = false;
	for (wchar_t c : in) {
		if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
		    (c >= L'0' && c <= L'9') || c == L'_' || c == L'-' || c == L'+') {
			out += c;
		} else if (c < 128) {
			out += L'_';
		} else {

			dropped = true;
		}
	}
	if (out.empty()) out = L"addin";
	if (out[0] >= L'0' && out[0] <= L'9') out = L"a" + out;
	if (dropped) {

		unsigned int h = 2166136261u;
		for (wchar_t c : in) { h ^= (unsigned int)c; h *= 16777619u; }
		wchar_t hex[16];
		swprintf(hex, 16, L"_%06X", h & 0xFFFFFFu);
		out += hex;
	}
	return out;
}

static std::wstring ToForwardSlashes(const std::wstring& s) {
	std::wstring r = s;
	for (auto& c : r) if (c == L'\\') c = L'/';
	return r;
}

static std::wstring BytesToWide(const std::string& s) {
	if (s.empty()) return L"";
	int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), NULL, 0);
	if (n > 0) {
		std::wstring w(n, 0);
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), &w[0], n);
		return w;
	}
	n = MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), NULL, 0);
	if (n > 0) {
		std::wstring w(n, 0);
		MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), &w[0], n);
		return w;
	}
	std::wstring w;
	for (unsigned char c : s) w += (wchar_t)c;
	return w;
}

static void LogClear() {
	g_logBuf.clear();
	if (g_hEditLog) {
		SetWindowTextW(g_hEditLog, L"");

		InvalidateRect(g_hEditLog, NULL, TRUE);
		UpdateWindow(g_hEditLog);
	}
}

static void LogAppend(const std::wstring& text) {

	std::wstring fixed;
	fixed.reserve(text.size() + 8);
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] == L'\n' && (i == 0 || text[i - 1] != L'\r')) fixed += L'\r';
		fixed += text[i];
	}
	g_logBuf += fixed;

	if (g_headless || !g_hEditLog) {

		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD type = hOut ? GetFileType(hOut) : FILE_TYPE_UNKNOWN;
		if (hOut && type == FILE_TYPE_CHAR) {
			DWORD w = 0;
			WriteConsoleW(hOut, fixed.c_str(), (DWORD)fixed.size(), &w, NULL);
		}
		return;
	}

	int len = GetWindowTextLengthW(g_hEditLog);
	SendMessageW(g_hEditLog, EM_SETSEL, len, len);
	SendMessageW(g_hEditLog, EM_REPLACESEL, FALSE, (LPARAM)fixed.c_str());
	SendMessageW(g_hEditLog, EM_SCROLLCARET, 0, 0);
}

static void SetStatus(const std::wstring& s) {
	if (g_hStatus) SetWindowTextW(g_hStatus, s.c_str());
}

struct ProcResult { DWORD code; bool started; std::string output; };

static ProcResult RunCapture(const std::wstring& exe, const std::wstring& args,
                             const std::wstring& workDir) {
	ProcResult r; r.code = (DWORD)-1; r.started = false;

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	HANDLE hRead = NULL, hWrite = NULL;
	if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return r;
	SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

	HANDLE hNul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                          &sa, OPEN_EXISTING, 0, NULL);

	std::wstring cmd = L"\"" + exe + L"\" " + args;
	std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
	cmdBuf.push_back(0);

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdInput = hNul;
	si.hStdOutput = hWrite;
	si.hStdError = hWrite;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	BOOL ok = CreateProcessW(exe.c_str(), cmdBuf.data(), NULL, NULL, TRUE,
	                         CREATE_NO_WINDOW, NULL,
	                         workDir.empty() ? NULL : workDir.c_str(), &si, &pi);

	CloseHandle(hWrite);
	if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);

	if (!ok) {
		r.output = "CreateProcess failed, error " + std::to_string(GetLastError());
		CloseHandle(hRead);
		return r;
	}
	r.started = true;

	char buf[4096];
	for (;;) {
		DWORD avail = 0;
		if (!PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL)) break;
		if (avail > 0) {
			DWORD got = 0;
			if (!ReadFile(hRead, buf, sizeof(buf) - 1, &got, NULL) || got == 0) break;
			r.output.append(buf, got);
		} else {
			if (WaitForSingleObject(pi.hProcess, 20) == WAIT_OBJECT_0) {
				DWORD got = 0;
				while (ReadFile(hRead, buf, sizeof(buf) - 1, &got, NULL) && got > 0)
					r.output.append(buf, got);
				break;
			}
		}
		MSG msg;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, &r.code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(hRead);
	return r;
}

/* ---- project folder support --------------------------------------------
   A single .cpp is one file, but a real console program is a folder with
   several translation units and a couple of headers.  The Makefile already
   compiles every .cpp it finds in src\, so all the builder has to do is copy
   the whole project in and make sure the file holding main() ends up called
   <project>.cpp, which is the one the special objcopy rule renames. */

static std::wstring FolderName(const std::wstring& path) {
	std::wstring p = path;
	while (!p.empty() && (p[p.size() - 1] == L'\\' || p[p.size() - 1] == L'/'))
		p.erase(p.size() - 1);
	size_t slash = p.find_last_of(L"\\/");
	return (slash == std::wstring::npos) ? p : p.substr(slash + 1);
}

static std::wstring ToLower(const std::wstring& s) {
	std::wstring r = s;
	for (auto& c : r) if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
	return r;
}

/* Absolute, slash normalised form of a path, so two spellings of the same file
   compare equal.  FullPathNameW also turns forward slashes into backslashes. */
static std::wstring FullPathOf(const std::wstring& p) {
	wchar_t buf[MAX_PATH * 2];
	DWORD n = GetFullPathNameW(p.c_str(), (DWORD)(sizeof(buf) / sizeof(buf[0])), buf, NULL);
	if (n == 0 || n >= (DWORD)(sizeof(buf) / sizeof(buf[0]))) return p;
	return std::wstring(buf, n);
}

static bool SamePath(const std::wstring& a, const std::wstring& b) {
	return ToLower(FullPathOf(a)) == ToLower(FullPathOf(b));
}

static std::wstring ExtOf(const std::wstring& name) {
	std::wstring low = ToLower(name);
	size_t dot = low.find_last_of(L'.');
	return (dot == std::wstring::npos) ? std::wstring() : low.substr(dot);
}

/* Everything worth copying into src\.  The Makefile only compiles .c and .cpp,
   so a stray .exe, .o or project file next to the sources is left behind. */
static bool IsProjectFile(const std::wstring& name) {
	std::wstring e = ExtOf(name);
	return e == L".cpp" || e == L".cxx" || e == L".cc" || e == L".c" ||
	       e == L".h"   || e == L".hpp" || e == L".hxx" || e == L".inl" ||
	       e == L".ipp" || e == L".s";
}

static bool IsCompilableFile(const std::wstring& name) {
	std::wstring e = ExtOf(name);
	return e == L".cpp" || e == L".cxx" || e == L".cc" || e == L".c";
}

/* Gather the project files, flattening sub directories: the Makefile globs
   src\*.cpp with no recursion, so a nested layout has to be flattened. */
static void CollectProjectFiles(const std::wstring& dir,
                                std::vector<std::wstring>& full,
                                std::vector<std::wstring>& names) {
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(Join(dir, L"*").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do {
		std::wstring nm = fd.cFileName;
		if (nm == L"." || nm == L"..") continue;
		std::wstring f = Join(dir, nm);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			CollectProjectFiles(f, full, names);
		} else if (IsProjectFile(nm)) {
			full.push_back(f);
			names.push_back(nm);
		}
	} while (FindNextFileW(h, &fd));
	FindClose(h);
}

/* Does this translation unit define main()?  Looks for the identifier at the
   start of a name, followed by an optional space and an open parenthesis,
   skipping anything after a // on the line. */
static bool FileDefinesMain(const std::string& code) {
	for (size_t i = 0; (i = code.find("main", i)) != std::string::npos; i += 4) {
		if (i > 0) {
			char p = code[i - 1];
			if ((p >= 'a' && p <= 'z') || (p >= 'A' && p <= 'Z') ||
			    (p >= '0' && p <= '9') || p == '_') continue;
		}
		size_t j = i + 4;
		while (j < code.size() && (code[j] == ' ' || code[j] == '\t')) j++;
		if (j >= code.size() || code[j] != '(') continue;
		size_t ls = code.rfind('\n', i);
		ls = (ls == std::string::npos) ? 0 : ls + 1;
		if (code.substr(ls, i - ls).find("//") != std::string::npos) continue;
		return true;
	}
	return false;
}

static bool DoBuild(const std::wstring& srcPath) {
	if (!PathExists(srcPath)) {
		MessageBoxW(g_hWnd, L"Source file or project folder not found.", L"Error", MB_ICONERROR);
		return false;
	}
	/* A folder is a whole project, a single file is one translation unit. */
	bool isProject = IsDir(srcPath);

	std::wstring origName = isProject ? FolderName(srcPath) : BaseNameNoExt(srcPath);
	std::wstring name = SanitizeName(origName);
	std::wstring sdkDir   = Join(g_root, L"sdk");
	std::wstring toolDir  = Join(g_root, L"toolchain");
	std::wstring iconsDir = Join(g_root, L"icons");
	std::wstring srcDir   = Join(g_root, L"src");
	std::wstring outDir   = Join(g_root, L"out");
	std::wstring tmpDir   = Join(g_root, L"tmp");
	std::wstring workDir  = Join(tmpDir, name);

	LogClear();
	LogAppend(L"cpprizm - Casio Prizm C++ builder\r\n");
	LogAppend(std::wstring(isProject ? L"project: " : L"source : ") + srcPath + L"\r\n");
	LogAppend(L"target : " + name + L"\r\n");
	if (origName != name)
		LogAppend(std::wstring(L"note   : the ") + (isProject ? L"folder" : L"file") +
		          L" name has non-ASCII characters, project name is " + name + L"\r\n");
	LogAppend(std::wstring(L"console: ") + FontDesc(g_conFont) + L", " + BgDesc(g_conBg) + L"\r\n");
	LogAppend(L"version: " + g_g3aVersion + L"\r\n");
	LogAppend(L"\r\n");

	std::wstring makeExe = Join(Join(sdkDir, L"bin"), L"make.exe");
	if (!PathExists(makeExe)) {
		LogAppend(L"error: sdk\\bin\\make.exe not found, the SDK directory is incomplete\r\n");
		SetStatus(L"failed: incomplete SDK");
		return false;
	}
	const wchar_t* need[] = { L"prizm_cpp.x", L"prizm_support.cpp", L"prizm_console.cpp",
	                          L"prizm_console_layout.h", L"prizm_rt.cpp",
	                          L"Makefile.template" };
	for (auto f : need) {
		if (!PathExists(Join(toolDir, f))) {
			LogAppend(std::wstring(L"error: toolchain\\") + f + L" is missing\r\n");
			SetStatus(L"failed: incomplete toolchain");
			return false;
		}
	}
	if (!IsDir(Join(toolDir, L"cppshim"))) {
		LogAppend(L"error: toolchain\\cppshim\\ is missing\r\n");
		SetStatus(L"failed: incomplete toolchain");
		return false;
	}
	if (!IsDir(Join(toolDir, L"cpplib"))) {
		LogAppend(L"error: toolchain\\cpplib\\ is missing\r\n");
		SetStatus(L"failed: incomplete toolchain");
		return false;
	}

	/* Work out what makes up the program.  For a single file that is just the
	   file, for a project folder it is every source and header under it, with
	   the one holding main() renamed to <name>.cpp, which is the translation
	   unit the special objcopy rule in the Makefile renames. */
	std::vector<std::wstring> files, fnames;
	std::wstring entry;
	bool entryFound = false;

	if (isProject) {
		CollectProjectFiles(srcPath, files, fnames);
		if (files.empty()) {
			LogAppend(L"error: no .cpp, .c or .h files in the project folder\r\n");
			SetStatus(L"failed: empty project");
			return false;
		}
		for (size_t i = 0; i < fnames.size(); i++)
			for (size_t j = i + 1; j < fnames.size(); j++)
				if (ToLower(fnames[i]) == ToLower(fnames[j])) {
					LogAppend(L"error: two project files are both named " + fnames[i] +
					          L"; the flat src\\ layout cannot keep them apart\r\n");
					SetStatus(L"failed: duplicate file name");
					return false;
				}

		std::string body;
		for (size_t i = 0; i < files.size(); i++) {
			if (!IsCompilableFile(fnames[i])) continue;
			if (!ReadAllBytes(files[i], body)) continue;
			if (FileDefinesMain(body)) {
				entry = files[i];
				entryFound = true;
				LogAppend(L"entry  : " + fnames[i] + L"  (defines main)\r\n");
				break;
			}
		}
		if (!entryFound) {
			for (size_t i = 0; i < files.size(); i++)
				if (IsCompilableFile(fnames[i])) { entry = files[i]; break; }
			LogAppend(L"warning: no file defines main(), a Prizm add-in needs int main(),"
			          L" using the first source anyway\r\n");
		}
		for (size_t i = 0; i < files.size(); i++)
			if (files[i] != entry && ToLower(fnames[i]) == ToLower(name + L".cpp")) {
				LogAppend(L"error: the project already has a file called " + name +
				          L".cpp, the name the entry point has to take\r\n");
				SetStatus(L"failed: entry name clash");
				return false;
			}
		LogAppend(L"files  : " + std::to_wstring(files.size()) +
		          L" copied into the work directory\r\n");
	} else {
		std::string body;
		if (!ReadAllBytes(srcPath, body) || body.empty()) {
			LogAppend(L"error: source file is unreadable or empty\r\n");
			SetStatus(L"failed: empty source");
			return false;
		}
		if (!FileDefinesMain(body))
			LogAppend(L"warning: no main found, a Prizm add-in needs int main(), still trying\r\n");
		files.push_back(srcPath);
		fnames.push_back(name + L".cpp");
		entry = srcPath;
	}

	CreateDirDeep(outDir);
	if (!isProject) {
		CreateDirDeep(srcDir);
		std::wstring keep = Join(srcDir, name + L".cpp");
		if (SamePath(srcPath, keep)) {
			/* The source already lives in src\, which is where a single file is
			   kept.  Copying a file onto itself fails on Windows, and there is
			   nothing to do anyway. */
			LogAppend(L"source kept  : src\\" + name + L".cpp  (already in place)\r\n");
		} else if (!CopyFileW(srcPath.c_str(), keep.c_str(), FALSE)) {
			LogAppend(L"error: cannot copy the source into src\\\r\n");
			SetStatus(L"failed: copy source");
			return false;
		} else {
			LogAppend(L"source saved : src\\" + name + L".cpp\r\n");
		}
	}

	if (IsDir(workDir)) DeleteDirDeep(workDir);
	if (!CreateDirDeep(Join(workDir, L"src"))) {
		LogAppend(L"error: cannot create the temporary work directory\r\n");
		SetStatus(L"failed: create directory");
		return false;
	}
	for (size_t i = 0; i < files.size(); i++) {
		std::wstring dstName = (files[i] == entry) ? (name + L".cpp") : fnames[i];
		if (!CopyFileW(files[i].c_str(), Join(Join(workDir, L"src"), dstName).c_str(), FALSE)) {
			LogAppend(L"error: cannot copy " + fnames[i] + L" into the work directory\r\n");
			SetStatus(L"failed: copy source");
			return false;
		}
	}
	CopyFileW(Join(toolDir, L"prizm_support.cpp").c_str(),
	          Join(Join(workDir, L"src"), L"prizm_support.cpp").c_str(), FALSE);
	CopyFileW(Join(toolDir, L"prizm_console.cpp").c_str(),
	          Join(Join(workDir, L"src"), L"prizm_console.cpp").c_str(), FALSE);
	CopyFileW(Join(toolDir, L"prizm_console_layout.h").c_str(),
	          Join(Join(workDir, L"src"), L"prizm_console_layout.h").c_str(), FALSE);
	CopyFileW(Join(toolDir, L"prizm_rt.cpp").c_str(),
	          Join(Join(workDir, L"src"), L"prizm_rt.cpp").c_str(), FALSE);
	CopyFileW(Join(toolDir, L"prizm_cpp.x").c_str(), Join(workDir, L"prizm_cpp.x").c_str(), FALSE);
	CopyFileW(Join(iconsDir, L"unselected.bmp").c_str(), Join(workDir, L"unselected.bmp").c_str(), FALSE);
	CopyFileW(Join(iconsDir, L"selected.bmp").c_str(), Join(workDir, L"selected.bmp").c_str(), FALSE);
	std::wstring shimDst = Join(workDir, L"cppshim");
	if (IsDir(shimDst)) DeleteDirDeep(shimDst);
	CopyDirDeep(Join(toolDir, L"cppshim"), shimDst);
	std::wstring libDst = Join(workDir, L"cpplib");
	if (IsDir(libDst)) DeleteDirDeep(libDst);
	CopyDirDeep(Join(toolDir, L"cpplib"), libDst);
	LogAppend(L"project directory ready : tmp\\" + name + L"\\\r\n");

	/* The console reads this, so the same prizm_console.cpp serves every
	   font and background combination the user can pick. */
	char cfgText[512];
	sprintf(cfgText,
	        "/* Written by cpprizm.exe.  Editing it works, but the next build\n"
	        "   overwrites it, so change the settings in the builder instead.\n"
	        "\n"
	        "     PRIZM_CON_FONT  1 = big, 2 = medium, 3 = small font\n"
	        "     PRIZM_CON_BG    0 = white paper with dark ink, 1 = the reverse */\n"
	        "#ifndef PRIZM_CONSOLE_CFG_H\n"
	        "#define PRIZM_CONSOLE_CFG_H\n"
	        "#define PRIZM_CON_FONT %d\n"
	        "#define PRIZM_CON_BG %d\n"
	        "#endif\n",
	        g_conFont, g_conBg);
	if (!WriteAllBytes(Join(Join(workDir, L"src"), L"prizm_console_cfg.h"), cfgText)) {
		LogAppend(L"error: cannot write prizm_console_cfg.h\r\n");
		SetStatus(L"failed: write console config");
		return false;
	}

	std::string tpl;
	if (!ReadAllBytes(Join(toolDir, L"Makefile.template"), tpl)) {
		LogAppend(L"error: cannot read Makefile.template\r\n");
		SetStatus(L"failed: template missing");
		return false;
	}
	std::wstring sdkFwdW = ToForwardSlashes(sdkDir);
	std::string sdkFwd;
	for (wchar_t c : sdkFwdW) sdkFwd += (char)c;
	std::string nameA;
	for (wchar_t c : name) nameA += (char)c;

	std::string verA;
	for (wchar_t c : g_g3aVersion) verA += (char)c;

	std::string mk;
	for (size_t i = 0; i < tpl.size();) {
		if (tpl.compare(i, 7, "__SDK__") == 0) { mk += sdkFwd; i += 7; }
		else if (tpl.compare(i, 10, "__TARGET__") == 0) { mk += nameA; i += 10; }
		else if (tpl.compare(i, 10, "__G3AVER__") == 0) { mk += verA; i += 10; }
		else { mk += tpl[i]; i++; }
	}
	if (!WriteAllBytes(Join(workDir, L"Makefile"), mk)) {
		LogAppend(L"error: cannot write the Makefile\r\n");
		SetStatus(L"failed: write Makefile");
		return false;
	}
	LogAppend(L"Makefile written\r\n\r\n");

	std::wstring sdkBin = Join(sdkDir, L"bin");
	SetEnvironmentVariableW(L"FXCGSDK", sdkDir.c_str());
	SetEnvironmentVariableW(L"PATH", (sdkBin + L";" + g_origPath).c_str());
	SetEnvironmentVariableW(L"CYGWIN", L"nodosfilewarning");

	LogAppend(L"compiling with sh3eb-elf-g++\r\n");
	SetStatus(L"compiling");
	UpdateWindow(g_hWnd);

	std::wstring args = L"-C \"" + workDir + L"\"";
	ProcResult pr = RunCapture(makeExe, args, workDir);

	LogAppend(BytesToWide(pr.output));
	if (!pr.output.empty() && pr.output.back() != '\n') LogAppend(L"\r\n");

	if (!pr.started) {
		LogAppend(L"\r\nerror: cannot start make.exe\r\n");
		SetStatus(L"failed: cannot start make");
		return false;
	}
	if (pr.code != 0) {
		LogAppend(L"\r\nerror: build failed, make exit code " + std::to_wstring(pr.code) + L"\r\n");
		SetStatus(L"build failed");
		return false;
	}

	std::wstring g3a = Join(workDir, name + L".g3a");
	if (!PathExists(g3a)) {
		LogAppend(L"\r\nerror: make succeeded but " + name + L".g3a was not produced\r\n");
		SetStatus(L"failed: no output");
		return false;
	}
	struct Item { const wchar_t* ext; };
	const wchar_t* exts[] = { L".g3a", L".bin", L".map" };
	for (auto e : exts) {
		std::wstring from = Join(workDir, name + std::wstring(e));
		if (PathExists(from))
			CopyFileW(from.c_str(), Join(outDir, name + std::wstring(e)).c_str(), FALSE);
	}
	LARGE_INTEGER li; li.QuadPart = 0;
	HANDLE hf = CreateFileW(Join(outDir, name + L".g3a").c_str(), GENERIC_READ, FILE_SHARE_READ,
	                        NULL, OPEN_EXISTING, 0, NULL);
	if (hf != INVALID_HANDLE_VALUE) { GetFileSizeEx(hf, &li); CloseHandle(hf); }
	LogAppend(L"\r\nbuilt : out\\" + name + L".g3a   (" + std::to_wstring(li.QuadPart) + L" bytes)\r\n");
	LogAppend(isProject ? (L"source: " + srcPath + L"\r\n")
	                    : (L"source: src\\" + name + L".cpp\r\n"));
	LogAppend(L"map   : out\\" + name + L".map\r\n");
	LogAppend(L"\r\ncopy the .g3a to the Prizm and run it\r\n");
	LogAppend(L"(FA-124, the official link software, or Memory Manager on the calculator)\r\n");
	SetStatus(L"ok: " + name + L".g3a");
	return true;
}

static void BrowseFile() {
	wchar_t buf[MAX_PATH] = L"";
	wchar_t initDir[MAX_PATH] = L"";
	if (GetEnvironmentVariableW(L"USERPROFILE", initDir, MAX_PATH)) {

		std::wstring d = Join(initDir, L"Desktop");
		if (IsDir(d)) {
			wcsncpy(initDir, d.c_str(), MAX_PATH - 1);
			initDir[MAX_PATH - 1] = 0;
		}
	}

	OPENFILENAMEW ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFilter = L"C++ source (*.cpp;*.cxx;*.cc)\0*.cpp;*.cxx;*.cc\0All files (*.*)\0*.*\0\0";
	ofn.lpstrFile = buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = initDir[0] ? initDir : NULL;
	ofn.lpstrTitle = L"Choose a C++ source file";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

	if (GetOpenFileNameW(&ofn)) {
		SetWindowTextW(g_hEditSrc, buf);
		SetStatus(L"source selected, click Build");
	}
}

/* Pick a whole project folder instead of one file.  A console program that
   came from a PC is usually a folder, and dropping the folder on the window
   does the same thing. */
static void BrowseFolder() {
	/* The modern folder dialog needs COM; the older one does not, but the
	   modern one is worth it and CoInitialize is harmless if already done. */
	CoInitialize(NULL);

	BROWSEINFOW bi;
	ZeroMemory(&bi, sizeof(bi));
	bi.hwndOwner = g_hWnd;
	bi.pszDisplayName = NULL;
	bi.lpszTitle = L"Choose the project folder (it needs a .cpp that defines main)";
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
	bi.lpfn = NULL;
	bi.lParam = 0;

	LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
	if (!pidl) return;
	wchar_t path[MAX_PATH] = L"";
	if (SHGetPathFromIDListW(pidl, path)) {
		SetWindowTextW(g_hEditSrc, path);
		SetStatus(L"project folder selected, click Build");
	}
	CoTaskMemFree(pidl);
}

static void OpenFolder(const std::wstring& folder) {
	if (!IsDir(folder)) CreateDirDeep(folder);
	ShellExecuteW(g_hWnd, L"open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

static void ShowConsoleChoice() {
	SetStatus(std::wstring(L"console: ") + FontDesc(g_conFont) + L", " + BgDesc(g_conBg));
}

static void OnBuildClicked() {
	wchar_t buf[MAX_PATH * 2] = L"";
	GetWindowTextW(g_hEditSrc, buf, MAX_PATH * 2);
	std::wstring src = buf;
	if (src.empty()) {
		MessageBoxW(g_hWnd, L"Pick a .cpp file or a project folder first.", L"Notice", MB_ICONINFORMATION);
		BrowseFile();
		GetWindowTextW(g_hEditSrc, buf, MAX_PATH * 2);
		src = buf;
		if (src.empty()) return;
	}

	wchar_t ver[64] = L"";
	GetWindowTextW(g_hEditVer, ver, 64);
	if (!ParseVersionOpt(ver, g_g3aVersion)) {
		MessageBoxW(g_hWnd,
			L"The add-in version must be 1 to 16 characters from\n"
			L"letters, digits, dots, dashes and underscores.\n\n"
			L"For example: 1.00  or  2.5.1-beta",
			L"Bad version", MB_ICONWARNING);
		SetFocus(g_hEditVer);
		return;
	}

	EnableWindow(GetDlgItem(g_hWnd, IDC_BTN_BUILD), FALSE);
	EnableWindow(GetDlgItem(g_hWnd, IDC_BTN_BROWSE), FALSE);
	DoBuild(src);
	EnableWindow(GetDlgItem(g_hWnd, IDC_BTN_BROWSE), TRUE);
	EnableWindow(GetDlgItem(g_hWnd, IDC_BTN_BUILD), TRUE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
	case WM_CREATE: {
		int x = 16, y = 16;

		HWND h = CreateWindowExW(0, L"STATIC", L"cpprizm - Casio Prizm C++ add-in builder",
			WS_CHILD | WS_VISIBLE, x, y, 640, 30, hwnd, NULL, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
		y += 38;

		h = CreateWindowExW(0, L"STATIC",
			L"Pick a C++ source file to build into a .g3a add-in.",
			WS_CHILD | WS_VISIBLE, x, y, 660, 20, hwnd, NULL, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
		y += 30;

		h = CreateWindowExW(0, L"STATIC", L"Source / project:",
			WS_CHILD | WS_VISIBLE, x, y + 3, 100, 20, hwnd, NULL, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		g_hEditSrc = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
			x + 104, y, 310, 24, hwnd, (HMENU)IDC_EDIT_SRC, g_hInst, NULL);
		SendMessageW(g_hEditSrc, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"BUTTON", L"File...",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
			x + 418, y - 1, 66, 26, hwnd, (HMENU)IDC_BTN_BROWSE, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"BUTTON", L"Folder...",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
			x + 488, y - 1, 76, 26, hwnd, (HMENU)IDC_BTN_BROWSEDIR, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"BUTTON", L"Build",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
			x + 570, y - 1, 100, 26, hwnd, (HMENU)IDC_BTN_BUILD, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
		y += 38;

		h = CreateWindowExW(0, L"STATIC", L"Console font:",
			WS_CHILD | WS_VISIBLE, x, y + 3, 100, 20, hwnd, NULL, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"BUTTON", L"Big  PrintXY  24 px",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
			x + 104, y, 160, 24, hwnd, (HMENU)IDC_RADIO_FONT1, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"BUTTON", L"Medium  PrintMini  18 px",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
			x + 268, y, 190, 24, hwnd, (HMENU)IDC_RADIO_FONT2, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"BUTTON", L"Small  PrintMiniMini  10 px",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
			x + 462, y, 210, 24, hwnd, (HMENU)IDC_RADIO_FONT3, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		SendMessageW(GetDlgItem(hwnd, IDC_RADIO_FONT2), BM_SETCHECK, BST_CHECKED, 0);
		y += 30;

		h = CreateWindowExW(0, L"STATIC", L"Background:",
			WS_CHILD | WS_VISIBLE, x, y + 3, 100, 20, hwnd, NULL, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"BUTTON", L"White paper",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
			x + 104, y, 130, 24, hwnd, (HMENU)IDC_RADIO_BGW, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"BUTTON", L"Black paper",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
			x + 238, y, 130, 24, hwnd, (HMENU)IDC_RADIO_BGB, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		SendMessageW(GetDlgItem(hwnd, IDC_RADIO_BGW), BM_SETCHECK, BST_CHECKED, 0);

		h = CreateWindowExW(0, L"STATIC", L"baked into the .g3a, rebuild to change",
			WS_CHILD | WS_VISIBLE, x + 374, y + 3, 290, 20, hwnd, NULL, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
		y += 30;

		h = CreateWindowExW(0, L"STATIC", L"Add-in version:",
			WS_CHILD | WS_VISIBLE, x, y + 3, 100, 20, hwnd, NULL, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		g_hEditVer = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1.00",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
			x + 104, y, 110, 24, hwnd, (HMENU)IDC_EDIT_VER, g_hInst, NULL);
		SendMessageW(g_hEditVer, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"STATIC", L"shown in the calculator main menu, 1..16 chars",
			WS_CHILD | WS_VISIBLE, x + 224, y + 3, 440, 20, hwnd, NULL, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
		y += 30;

		h = CreateWindowExW(0, L"STATIC", L"Log:",
			WS_CHILD | WS_VISIBLE, x, y, 60, 20, hwnd, NULL, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
		y += 22;

		g_hEditLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
			ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
			x, y, 712, 300, hwnd, (HMENU)IDC_EDIT_LOG, g_hInst, NULL);
		SendMessageW(g_hEditLog, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
		y += 310;

		h = CreateWindowExW(0, L"BUTTON", L"Output folder",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
			x, y, 140, 28, hwnd, (HMENU)IDC_BTN_OPENDIR, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		h = CreateWindowExW(0, L"BUTTON", L"Source folder",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
			x + 150, y, 140, 28, hwnd, (HMENU)IDC_BTN_OPENSRC, g_hInst, NULL);
		SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		g_hStatus = CreateWindowExW(0, L"STATIC", L"ready",
			WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
			x + 300, y + 5, 420, 20, hwnd, (HMENU)IDC_STATIC_STATUS, g_hInst, NULL);
		SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

		DragAcceptFiles(hwnd, TRUE);

		LogClear();
		LogAppend(L"cpprizm - Casio Prizm C++ add-in builder\r\n\r\n");
		LogAppend(L"Pick a .cpp file, or a whole project folder, and build a .g3a add-in.\r\n");
		LogAppend(L"Use File... for one source, Folder... for a multi-file project, or just\r\n");
		LogAppend(L"drag either one onto this window.  In a folder cpprizm finds the .cpp\r\n");
		LogAppend(L"that defines main() and makes it the entry point of the add-in.\r\n\r\n");
		LogAppend(L"#include <iostream> works, cout / cin write to the on-screen console.\r\n");
		LogAppend(L"Supported: classes, virtual functions, templates, new / delete, global objects,\r\n");
		LogAppend(L"printf, std::string, the STL containers and <algorithm>, smart pointers,\r\n");
		LogAppend(L"<sstream>, <random>, <complex>, <chrono>, windows.h console calls,\r\n");
		LogAppend(L"rand / time / the math library.\r\n");
		LogAppend(L"Not supported: exceptions, RTTI, threads, files, there is no libstdc++\r\n");
		LogAppend(L"on the Prizm.\r\n\r\n");
		LogAppend(L"A single file is copied to src\\, a project folder is used where it is.\r\n");
		LogAppend(L"The .g3a always goes to out\\.\r\n");
		LogAppend(L"Pick the console font and the console background above before building.\r\n");
		LogAppend(L"Both are compiled into the .g3a, so rebuild to change them.\r\n");
		return 0;
	}

	case WM_DROPFILES: {
		HDROP hd = (HDROP)wp;
		wchar_t path[MAX_PATH * 2] = L"";
		if (DragQueryFileW(hd, 0, path, MAX_PATH * 2)) {
			SetWindowTextW(g_hEditSrc, path);
			SetStatus(IsDir(path) ? L"project folder selected, click Build"
			                      : L"source selected, click Build");
		}
		DragFinish(hd);
		SetForegroundWindow(hwnd);
		return 0;
	}

	case WM_COMMAND: {
		switch (LOWORD(wp)) {
		case IDC_BTN_BROWSE: BrowseFile(); return 0;
		case IDC_BTN_BROWSEDIR: BrowseFolder(); return 0;
		case IDC_BTN_BUILD:  OnBuildClicked(); return 0;
		case IDC_BTN_OPENDIR: OpenFolder(Join(g_root, L"out")); return 0;
		case IDC_BTN_OPENSRC: OpenFolder(Join(g_root, L"src")); return 0;
		case IDC_RADIO_FONT1: g_conFont = 1; ShowConsoleChoice(); return 0;
		case IDC_RADIO_FONT2: g_conFont = 2; ShowConsoleChoice(); return 0;
		case IDC_RADIO_FONT3: g_conFont = 3; ShowConsoleChoice(); return 0;
		case IDC_RADIO_BGW:   g_conBg   = 0; ShowConsoleChoice(); return 0;
		case IDC_RADIO_BGB:   g_conBg   = 1; ShowConsoleChoice(); return 0;
		}
		return 0;
	}

	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLOREDIT: {
		HWND hCtl = (HWND)lp;
		HDC dc = (HDC)wp;
		if (hCtl == g_hEditLog || hCtl == g_hEditSrc || hCtl == g_hEditVer) {
			SetBkMode(dc, OPAQUE);
			SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
			SetBkColor(dc, GetSysColor(COLOR_WINDOW));
			return (LRESULT)(g_hBrushLog ? g_hBrushLog
			                             : GetSysColorBrush(COLOR_WINDOW));
		}
		SetBkMode(dc, TRANSPARENT);
		return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
	}

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY: {
		HFONT fUI = g_hFontUI, fMono = g_hFontMono;
		g_hFontUI = g_hFontMono = nullptr;
		if (fUI) DeleteObject(fUI);
		if (fMono && fMono != fUI) DeleteObject(fMono);
		if (g_hBrushLog) { DeleteObject(g_hBrushLog); g_hBrushLog = nullptr; }
		PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
}

static void InitPaths() {
	wchar_t exePath[MAX_PATH * 2] = L"";
	GetModuleFileNameW(NULL, exePath, MAX_PATH * 2);
	std::wstring p = exePath;
	size_t slash = p.find_last_of(L"\\/");
	g_root = (slash == std::wstring::npos) ? L"." : p.substr(0, slash);

	wchar_t oldPath[32767] = L"";
	if (GetEnvironmentVariableW(L"PATH", oldPath, 32767)) g_origPath = oldPath;
}

static void WriteLogFile(const std::wstring& logPath) {
	CreateDirDeep(Join(g_root, L"tmp"));
	std::string utf8;
	int n = WideCharToMultiByte(CP_UTF8, 0, g_logBuf.c_str(), (int)g_logBuf.size(), NULL, 0, NULL, NULL);
	if (n > 0) {
		utf8.resize(n);
		WideCharToMultiByte(CP_UTF8, 0, g_logBuf.c_str(), (int)g_logBuf.size(), &utf8[0], n, NULL, NULL);
	}
	std::string withBom = "\xEF\xBB\xBF" + utf8;
	WriteAllBytes(logPath, withBom);
}

static std::vector<std::wstring> SplitArgs(const std::wstring& cl) {
	std::vector<std::wstring> toks;
	std::wstring cur;
	bool inQ = false;
	for (wchar_t c : cl) {
		if (c == L'"') { inQ = !inQ; continue; }
		if (!inQ && (c == L' ' || c == L'\t')) {
			if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
		} else cur += c;
	}
	if (!cur.empty()) toks.push_back(cur);
	return toks;
}

static bool ParseFontOpt(const std::wstring& v, int& out) {
	if (v == L"big"    || v == L"1") { out = 1; return true; }
	if (v == L"medium" || v == L"2") { out = 2; return true; }
	if (v == L"small"  || v == L"3") { out = 3; return true; }
	return false;
}

static bool ParseBgOpt(const std::wstring& v, int& out) {
	if (v == L"white" || v == L"0") { out = 0; return true; }
	if (v == L"black" || v == L"1") { out = 1; return true; }
	return false;
}

static const wchar_t* kUsage =
	L"usage: cpprizm --build <source.cpp | project folder>\r\n"
	L"                     [--log <out.txt>]\r\n"
	L"                     [--font big|medium|small] [--bg white|black]\r\n"
	L"                     [--version <string>]\r\n";

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR lpCmdLine, int nCmdShow) {
	g_hInst = hInst;
	InitPaths();

	std::wstring cl = lpCmdLine ? lpCmdLine : L"";
	if (cl.find(L"--build") != std::wstring::npos) {
		g_headless = true;
		AttachConsole(ATTACH_PARENT_PROCESS);

		std::vector<std::wstring> toks = SplitArgs(cl);
		std::wstring target, logPath;
		for (size_t i = 0; i < toks.size(); i++) {
			if (toks[i] == L"--build" && i + 1 < toks.size()) target = toks[i + 1];
			else if (toks[i] == L"--log" && i + 1 < toks.size()) logPath = toks[i + 1];
			else if (toks[i] == L"--font" && i + 1 < toks.size()) {
				if (!ParseFontOpt(toks[++i], g_conFont))
					g_optError = std::wstring(L"--font expects big, medium or small, not '") + toks[i] + L"'";
			}
			else if (toks[i] == L"--bg" && i + 1 < toks.size()) {
				if (!ParseBgOpt(toks[++i], g_conBg))
					g_optError = std::wstring(L"--bg expects white or black, not '") + toks[i] + L"'";
			}
			else if (toks[i] == L"--version" && i + 1 < toks.size()) {
				if (!ParseVersionOpt(toks[++i], g_g3aVersion))
					g_optError = std::wstring(L"--version expects 1 to 16 ASCII letters, digits, dots, dashes or underscores, not '") + toks[i] + L"'";
			}
			else if (toks[i] == L"--font") g_optError = L"--font needs a value: big, medium or small";
			else if (toks[i] == L"--bg")   g_optError = L"--bg needs a value: white or black";
			else if (toks[i] == L"--version") g_optError = L"--version needs a value, for example 1.00";
		}

		if (logPath.empty()) logPath = Join(g_root, L"tmp\\last-build.log");

		bool ok = false;
		if (!g_optError.empty()) {
			LogAppend(std::wstring(L"error: ") + g_optError + L"\r\n\r\n");
			LogAppend(kUsage);
		} else if (target.empty()) {
			LogAppend(kUsage);
		} else {
			ok = DoBuild(target);
		}

		WriteLogFile(logPath);
		return ok ? 0 : 1;
	}

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&icc);

	g_hFontUI = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH, L"Segoe UI");
	if (!g_hFontUI) g_hFontUI = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	g_hFontMono = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		FIXED_PITCH | FF_MODERN, L"Consolas");
	if (!g_hFontMono) g_hFontMono = g_hFontUI;

	g_hBrushLog = CreateSolidBrush(GetSysColor(COLOR_WINDOW));

	WNDCLASSEXW wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszClassName = L"PrizmCppBuilderWnd";
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassExW(&wc);

	int w = 762, hgt = 634;
	int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
	int sy = (GetSystemMetrics(SM_CYSCREEN) - hgt) / 2;

	g_hWnd = CreateWindowExW(0, L"PrizmCppBuilderWnd",
		L"cpprizm - Casio Prizm C++ add-in builder",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		sx, sy, w, hgt, NULL, NULL, hInst, NULL);
	if (!g_hWnd) return 1;

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	if (!cl.empty()) {
		std::wstring arg = cl;
		if (arg.front() == L'"' && arg.back() == L'"' && arg.size() > 2)
			arg = arg.substr(1, arg.size() - 2);
		SetWindowTextW(g_hEditSrc, arg.c_str());
		SetStatus(L"source selected, click Build");
	}

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return (int)msg.wParam;
}

#ifdef PRIZM_CLI
int wmain(int argc, wchar_t** argv) {
	InitPaths();
	g_headless = true;

	std::wstring target, logPath;
	for (int i = 1; i < argc; i++) {
		std::wstring a = argv[i];
		if (a == L"--log" && i + 1 < argc) { logPath = argv[++i]; }
		else if (a == L"--build" && i + 1 < argc) { target = argv[++i]; }
		else if (a == L"--font" && i + 1 < argc) {
			if (!ParseFontOpt(argv[++i], g_conFont))
				g_optError = std::wstring(L"--font expects big, medium or small, not '") + argv[i] + L"'";
		}
		else if (a == L"--bg" && i + 1 < argc) {
			if (!ParseBgOpt(argv[++i], g_conBg))
				g_optError = std::wstring(L"--bg expects white or black, not '") + argv[i] + L"'";
		}
		else if (a == L"--version" && i + 1 < argc) {
			if (!ParseVersionOpt(argv[++i], g_g3aVersion))
				g_optError = std::wstring(L"--version expects 1 to 16 ASCII letters, digits, dots, dashes or underscores, not '") + argv[i] + L"'";
		}
		else if (a == L"--font") g_optError = L"--font needs a value: big, medium or small";
		else if (a == L"--bg")   g_optError = L"--bg needs a value: white or black";
		else if (a == L"--version") g_optError = L"--version needs a value, for example 1.00";
		else if (a == L"--help" || a == L"-h") {
			wprintf(L"%ls", kUsage);
			return 0;
		}
		else if (!a.empty() && a[0] != L'-' && target.empty()) { target = a; }
	}

	if (!g_optError.empty()) {
		LogAppend(std::wstring(L"error: ") + g_optError + L"\r\n\r\n");
		LogAppend(kUsage);
		if (logPath.empty()) logPath = Join(g_root, L"tmp\\last-build.log");
		WriteLogFile(logPath);
		wprintf(L"error: %ls\n", g_optError.c_str());
		return 1;
	}

	bool ok = false;
	if (target.empty()) {
		wprintf(L"%ls", kUsage);
		return 1;
	}
	ok = DoBuild(target);

	if (logPath.empty()) logPath = Join(g_root, L"tmp\\last-build.log");
	WriteLogFile(logPath);
	wprintf(L"log: %ls\n", logPath.c_str());
	return ok ? 0 : 1;
}
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int IdTab = 100;
constexpr int IdInput = 101;
constexpr int IdBrowseInput = 102;
constexpr int IdOutput = 103;
constexpr int IdBrowseOutput = 104;
constexpr int IdCourseFormula = 105;
constexpr int IdScoreFormula = 106;
constexpr int IdGpaFormula = 107;
constexpr int IdComprehensiveFormula = 108;
constexpr int IdCalculate = 109;
constexpr int IdCreateTemplate = 110;
constexpr int IdOpenResult = 111;
constexpr int IdStatus = 112;
constexpr int IdResetFormulas = 113;
constexpr int IdFontSize = 114;
constexpr int IdTheme = 115;
constexpr int IdApplyUi = 116;
constexpr int IdOpenManual = 117;

const wchar_t* DefaultCourseFormula =
    L"if(score>=90,4.0,if(score>=85,3.7,if(score>=82,3.3,if(score>=78,3.0,"
    L"if(score>=75,2.7,if(score>=72,2.3,if(score>=68,2.0,if(score>=64,1.5,if(score>=60,1.0,0)))))))))";

HWND mainWindow = nullptr;
HWND tabControl = nullptr;
HWND inputEdit = nullptr;
HWND outputEdit = nullptr;
HWND courseFormulaEdit = nullptr;
HWND scoreFormulaEdit = nullptr;
HWND gpaFormulaEdit = nullptr;
HWND comprehensiveFormulaEdit = nullptr;
HWND statusEdit = nullptr;
HWND openResultButton = nullptr;
HWND fontSizeCombo = nullptr;
HWND themeCombo = nullptr;
HWND titleLabel = nullptr;
HFONT normalFont = nullptr;
HFONT titleFont = nullptr;
HBRUSH backgroundBrush = nullptr;
COLORREF backgroundColor = RGB(247, 249, 252);
COLORREF accentColor = RGB(35, 86, 155);
int interfaceFontSize = 17;
int interfaceTheme = 0;
std::vector<HWND> calculationPage;
std::vector<HWND> settingsPage;
std::vector<HWND> guidePage;
std::vector<HWND> allControls;
std::wstring lastOutputPath;
std::wstring startupInput;
std::wstring startupOutput;
bool startupAutoRun = false;

std::wstring getText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    if (length > 0) GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<std::size_t>(length));
    return value;
}

void setText(HWND control, const std::wstring& value) {
    SetWindowTextW(control, value.c_str());
}

std::string toUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                            nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring fromUtf8(const std::string& value) {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) return L"程序返回了无法显示的文本。";
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::wstring quoteArgument(const std::wstring& argument) {
    if (argument.find_first_of(L" \t\"") == std::wstring::npos) return argument;
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++slashes;
        } else if (character == L'\"') {
            result.append(slashes * 2 + 1, L'\\');
            result += L'\"';
            slashes = 0;
        } else {
            result.append(slashes, L'\\');
            slashes = 0;
            result += character;
        }
    }
    result.append(slashes * 2, L'\\');
    result += L'\"';
    return result;
}

fs::path moduleDirectory() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return fs::path(std::wstring(buffer.data(), length)).parent_path();
}

fs::path backendPath() {
    const fs::path directory = moduleDirectory();
    const fs::path deployed = directory / L"ClassRankerCLI.exe";
    if (fs::exists(deployed)) return deployed;

    const fs::path development = directory / L"ClassRanker.exe";
    std::vector<wchar_t> currentBuffer(32768);
    const DWORD currentLength = GetModuleFileNameW(nullptr, currentBuffer.data(), static_cast<DWORD>(currentBuffer.size()));
    const fs::path current(std::wstring(currentBuffer.data(), currentLength));
    if (fs::exists(development) && !fs::equivalent(current, development)) return development;
    return deployed;
}

struct ProcessResult {
    DWORD exitCode = 1;
    std::wstring output;
};

ProcessResult runBackend(const std::vector<std::wstring>& arguments) {
    const fs::path executable = backendPath();
    if (!fs::exists(executable)) {
        return {1, L"找不到后台计算程序 ClassRankerCLI.exe。请确认它与窗体程序位于同一目录。"};
    }

    std::wstring command = quoteArgument(executable.wstring());
    for (const auto& argument : arguments) command += L" " + quoteArgument(argument);
    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        return {GetLastError(), L"无法创建进程输出管道。"};
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const std::wstring workingDirectory = executable.parent_path().wstring();

    const BOOL created = CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, workingDirectory.c_str(), &startup, &process);
    CloseHandle(writePipe);
    if (!created) {
        const DWORD error = GetLastError();
        CloseHandle(readPipe);
        return {error, L"无法启动后台计算程序，Windows 错误代码：" + std::to_wstring(error)};
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    std::string output;
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        output.append(buffer, buffer + read);
    }
    CloseHandle(readPipe);
    return {exitCode, fromUtf8(output)};
}

void applyFont(HWND control, HFONT font = nullptr) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : normalFont), TRUE);
}

fs::path uiSettingsPath() {
    return moduleDirectory() / L"ui_settings.ini";
}

void setThemeColors() {
    switch (interfaceTheme) {
    case 1:
        backgroundColor = RGB(238, 246, 252);
        accentColor = RGB(20, 96, 142);
        break;
    case 2:
        backgroundColor = RGB(241, 248, 244);
        accentColor = RGB(35, 112, 79);
        break;
    default:
        backgroundColor = RGB(247, 249, 252);
        accentColor = RGB(35, 86, 155);
        break;
    }
    if (backgroundBrush) DeleteObject(backgroundBrush);
    backgroundBrush = CreateSolidBrush(backgroundColor);
}

void rebuildFonts() {
    HFONT newNormal = CreateFontW(-interfaceFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT newTitle = CreateFontW(-(interfaceFontSize + 10), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT oldNormal = normalFont;
    HFONT oldTitle = titleFont;
    normalFont = newNormal;
    titleFont = newTitle;
    for (HWND control : allControls) applyFont(control);
    if (titleLabel) applyFont(titleLabel, titleFont);
    if (oldNormal) DeleteObject(oldNormal);
    if (oldTitle) DeleteObject(oldTitle);
}

void loadUiSettings() {
    const fs::path path = uiSettingsPath();
    interfaceFontSize = GetPrivateProfileIntW(L"ui", L"font_size", 17, path.c_str());
    if (interfaceFontSize != 15 && interfaceFontSize != 17 && interfaceFontSize != 19) interfaceFontSize = 17;
    interfaceTheme = GetPrivateProfileIntW(L"ui", L"theme", 0, path.c_str());
    if (interfaceTheme < 0 || interfaceTheme > 2) interfaceTheme = 0;
    setThemeColors();
    rebuildFonts();
}

void saveUiSettings() {
    const fs::path path = uiSettingsPath();
    WritePrivateProfileStringW(L"ui", L"font_size", std::to_wstring(interfaceFontSize).c_str(), path.c_str());
    WritePrivateProfileStringW(L"ui", L"theme", std::to_wstring(interfaceTheme).c_str(), path.c_str());
}

HWND createControl(DWORD extendedStyle, const wchar_t* className, const wchar_t* text,
                   DWORD style, int x, int y, int width, int height, int id,
                   std::vector<HWND>* page = nullptr) {
    HWND control = CreateWindowExW(extendedStyle, className, text, style | WS_CHILD | WS_VISIBLE,
                                   x, y, width, height, mainWindow,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
    applyFont(control);
    allControls.push_back(control);
    if (page) page->push_back(control);
    return control;
}

HWND createLabel(const wchar_t* text, int x, int y, int width, int height, std::vector<HWND>& page) {
    return createControl(0, L"STATIC", text, SS_LEFT, x, y, width, height, 0, &page);
}

void setStatus(const std::wstring& value) {
    setText(statusEdit, value);
}

std::wstring selectOpenXlsx() {
    wchar_t fileName[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = mainWindow;
    dialog.lpstrFilter = L"Excel 工作簿 (*.xlsx)\0*.xlsx\0所有文件 (*.*)\0*.*\0";
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = static_cast<DWORD>(std::size(fileName));
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    return GetOpenFileNameW(&dialog) ? std::wstring(fileName) : std::wstring();
}

std::wstring selectSaveXlsx(const wchar_t* defaultName) {
    wchar_t fileName[32768]{};
    wcscpy_s(fileName, defaultName);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = mainWindow;
    dialog.lpstrFilter = L"Excel 工作簿 (*.xlsx)\0*.xlsx\0所有文件 (*.*)\0*.*\0";
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = static_cast<DWORD>(std::size(fileName));
    dialog.lpstrDefExt = L"xlsx";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    return GetSaveFileNameW(&dialog) ? std::wstring(fileName) : std::wstring();
}

fs::path createFormulaFile() {
    wchar_t tempDirectory[MAX_PATH]{};
    wchar_t tempFile[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tempDirectory);
    GetTempFileNameW(tempDirectory, L"CRF", 0, tempFile);
    const fs::path path(tempFile);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << "course_gpa_formula=" << toUtf8(getText(courseFormulaEdit)) << "\n"
           << "score_ranking_formula=" << toUtf8(getText(scoreFormulaEdit)) << "\n"
           << "gpa_ranking_formula=" << toUtf8(getText(gpaFormulaEdit)) << "\n"
           << "comprehensive_ranking_formula=" << toUtf8(getText(comprehensiveFormulaEdit)) << "\n";
    if (!stream) throw std::runtime_error("无法写入临时公式配置文件");
    return path;
}

void browseInput() {
    const std::wstring selected = selectOpenXlsx();
    if (selected.empty()) return;
    setText(inputEdit, selected);
    if (getText(outputEdit).empty()) {
        const fs::path input(selected);
        setText(outputEdit, (input.parent_path() / L"ranking_results.xlsx").wstring());
    }
}

void browseOutput() {
    const std::wstring selected = selectSaveXlsx(L"ranking_results.xlsx");
    if (!selected.empty()) setText(outputEdit, selected);
}

void createTemplate() {
    const std::wstring path = selectSaveXlsx(L"class_data_template.xlsx");
    if (path.empty()) return;
    setStatus(L"正在生成 Excel 输入模板...");
    const ProcessResult result = runBackend({L"--create-template", path});
    setStatus(result.output.empty() ? (result.exitCode == 0 ? L"模板生成完成。" : L"模板生成失败。") : result.output);
    if (result.exitCode == 0) {
        setText(inputEdit, path);
        const fs::path input(path);
        setText(outputEdit, (input.parent_path() / L"ranking_results.xlsx").wstring());
    } else {
        MessageBoxW(mainWindow, result.output.c_str(), L"模板生成失败", MB_OK | MB_ICONERROR);
    }
}

void calculateRanking() {
    const std::wstring input = getText(inputEdit);
    const std::wstring output = getText(outputEdit);
    if (input.empty() || output.empty()) {
        MessageBoxW(mainWindow, L"请选择输入 Excel 文件和结果保存位置。", L"缺少路径", MB_OK | MB_ICONWARNING);
        return;
    }
    if (getText(courseFormulaEdit).empty() || getText(scoreFormulaEdit).empty() ||
        getText(gpaFormulaEdit).empty() || getText(comprehensiveFormulaEdit).empty()) {
        MessageBoxW(mainWindow, L"四个计算公式都不能为空。", L"缺少公式", MB_OK | MB_ICONWARNING);
        return;
    }

    fs::path formulaFile;
    try {
        formulaFile = createFormulaFile();
        setStatus(L"正在读取数据并计算排名，请稍候...");
        EnableWindow(GetDlgItem(mainWindow, IdCalculate), FALSE);
        const ProcessResult result = runBackend({L"--input", input, L"--output", output,
                                                  L"--config", formulaFile.wstring(), L"--no-prompt"});
        EnableWindow(GetDlgItem(mainWindow, IdCalculate), TRUE);
        DeleteFileW(formulaFile.c_str());
        setStatus(result.output.empty() ? (result.exitCode == 0 ? L"计算完成。" : L"计算失败。") : result.output);
        if (result.exitCode == 0) {
            lastOutputPath = output;
            EnableWindow(openResultButton, TRUE);
            MessageBoxW(mainWindow, L"成绩、绩点和综测排名已经计算完成。", L"计算完成", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(mainWindow, result.output.c_str(), L"计算失败", MB_OK | MB_ICONERROR);
        }
    } catch (const std::exception& error) {
        if (!formulaFile.empty()) DeleteFileW(formulaFile.c_str());
        EnableWindow(GetDlgItem(mainWindow, IdCalculate), TRUE);
        const std::wstring message = fromUtf8(error.what());
        setStatus(message);
        MessageBoxW(mainWindow, message.c_str(), L"计算失败", MB_OK | MB_ICONERROR);
    }
}

void openResult() {
    if (lastOutputPath.empty()) return;
    const HINSTANCE result = ShellExecuteW(mainWindow, L"open", lastOutputPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        MessageBoxW(mainWindow, L"无法打开结果文件，请确认系统已安装 Excel 或其他表格软件。",
                    L"打开失败", MB_OK | MB_ICONERROR);
    }
}

void resetFormulas() {
    setText(courseFormulaEdit, DefaultCourseFormula);
    setText(scoreFormulaEdit, L"weighted_score");
    setText(gpaFormulaEdit, L"weighted_gpa");
    setText(comprehensiveFormulaEdit, L"weighted_score*0.8+bonus_total");
    MessageBoxW(mainWindow, L"四项公式已恢复为程序默认值。", L"恢复完成", MB_OK | MB_ICONINFORMATION);
}

void applyUiCustomization() {
    const int fontSelection = static_cast<int>(SendMessageW(fontSizeCombo, CB_GETCURSEL, 0, 0));
    const int themeSelection = static_cast<int>(SendMessageW(themeCombo, CB_GETCURSEL, 0, 0));
    interfaceFontSize = fontSelection == 0 ? 15 : (fontSelection == 2 ? 19 : 17);
    interfaceTheme = themeSelection >= 0 && themeSelection <= 2 ? themeSelection : 0;
    setThemeColors();
    rebuildFonts();
    saveUiSettings();
    RedrawWindow(mainWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    MessageBoxW(mainWindow, L"界面设置已应用并保存。", L"设置完成", MB_OK | MB_ICONINFORMATION);
}

void openManual() {
    const fs::path manual = moduleDirectory() / L"操作手册.txt";
    if (!fs::exists(manual)) {
        MessageBoxW(mainWindow, L"找不到操作手册.txt，请确认它与程序位于同一目录。",
                    L"文件缺失", MB_OK | MB_ICONERROR);
        return;
    }
    const HINSTANCE result = ShellExecuteW(mainWindow, L"open", manual.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        MessageBoxW(mainWindow, L"无法打开操作手册.txt。", L"打开失败", MB_OK | MB_ICONERROR);
    }
}

void showPage(int selected) {
    for (HWND control : calculationPage) ShowWindow(control, selected == 0 ? SW_SHOW : SW_HIDE);
    for (HWND control : settingsPage) ShowWindow(control, selected == 1 ? SW_SHOW : SW_HIDE);
    for (HWND control : guidePage) ShowWindow(control, selected == 2 ? SW_SHOW : SW_HIDE);
}

void createInterface() {
    loadUiSettings();

    titleLabel = createControl(0, L"STATIC", L"班级成绩与综测排名", SS_LEFT, 24, 16, 500, 36, 0);
    applyFont(titleLabel, titleFont);
    createControl(0, L"STATIC", L"支持 Excel 数据、可配置公式和三类排名导出", SS_LEFT,
                  26, 51, 620, 24, 0);

    tabControl = createControl(0, WC_TABCONTROLW, L"", WS_TABSTOP | WS_CLIPSIBLINGS,
                               18, 82, 864, 558, IdTab);
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<wchar_t*>(L"排名计算");
    TabCtrl_InsertItem(tabControl, 0, &item);
    item.pszText = const_cast<wchar_t*>(L"公式与界面");
    TabCtrl_InsertItem(tabControl, 1, &item);
    item.pszText = const_cast<wchar_t*>(L"绩点说明");
    TabCtrl_InsertItem(tabControl, 2, &item);

    const std::wstring defaultInput = (moduleDirectory() / L"class_data_template.xlsx").wstring();
    const std::wstring defaultOutput = (moduleDirectory() / L"ranking_results.xlsx").wstring();

    createControl(0, L"BUTTON", L" 数据文件 ", BS_GROUPBOX, 36, 119, 828, 112, 0, &calculationPage);
    createLabel(L"输入 Excel", 56, 145, 110, 24, calculationPage);
    inputEdit = createControl(WS_EX_CLIENTEDGE, L"EDIT", defaultInput.c_str(), ES_AUTOHSCROLL | WS_TABSTOP,
                              166, 141, 576, 30, IdInput, &calculationPage);
    createControl(0, L"BUTTON", L"浏览...", BS_PUSHBUTTON | WS_TABSTOP,
                  754, 140, 88, 31, IdBrowseInput, &calculationPage);
    createLabel(L"结果文件", 56, 187, 110, 24, calculationPage);
    outputEdit = createControl(WS_EX_CLIENTEDGE, L"EDIT", defaultOutput.c_str(), ES_AUTOHSCROLL | WS_TABSTOP,
                               166, 183, 576, 30, IdOutput, &calculationPage);
    createControl(0, L"BUTTON", L"浏览...", BS_PUSHBUTTON | WS_TABSTOP,
                  754, 182, 88, 31, IdBrowseOutput, &calculationPage);

    createControl(0, L"BUTTON", L" 主要操作 ", BS_GROUPBOX, 36, 246, 828, 116, 0, &calculationPage);
    createControl(0, L"BUTTON", L"生成输入模板", BS_PUSHBUTTON | WS_TABSTOP,
                  56, 277, 150, 38, IdCreateTemplate, &calculationPage);
    createControl(0, L"BUTTON", L"开始计算排名", BS_DEFPUSHBUTTON | WS_TABSTOP,
                  218, 277, 156, 38, IdCalculate, &calculationPage);
    openResultButton = createControl(0, L"BUTTON", L"打开结果", BS_PUSHBUTTON | WS_TABSTOP,
                                     386, 277, 130, 38, IdOpenResult, &calculationPage);
    EnableWindow(openResultButton, FALSE);
    createControl(0, L"BUTTON", L"打开操作手册", BS_PUSHBUTTON | WS_TABSTOP,
                  528, 277, 150, 38, IdOpenManual, &calculationPage);
    createLabel(L"需要调整绩点分段或综测权重时，请前往“公式与界面”页面。",
                56, 326, 660, 24, calculationPage);

    createControl(0, L"BUTTON", L" 运行状态 ", BS_GROUPBOX, 36, 378, 828, 240, 0, &calculationPage);
    statusEdit = createControl(WS_EX_CLIENTEDGE, L"EDIT",
                               L"已载入随程序附带的示例 Excel。可直接点击“开始计算排名”。\r\n\r\n"
                               L"提示：计算前请关闭 Excel 中已经打开的结果文件。",
                               ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                               54, 407, 790, 190, IdStatus, &calculationPage);
    SendMessageW(statusEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));

    createControl(0, L"BUTTON", L" 排名与绩点公式 ", BS_GROUPBOX, 36, 119, 828, 260, 0, &settingsPage);
    createLabel(L"课程绩点", 56, 151, 110, 24, settingsPage);
    courseFormulaEdit = createControl(WS_EX_CLIENTEDGE, L"EDIT", DefaultCourseFormula,
                                      ES_AUTOHSCROLL | WS_TABSTOP, 166, 147, 676, 30,
                                      IdCourseFormula, &settingsPage);
    createLabel(L"成绩排名", 56, 191, 110, 24, settingsPage);
    scoreFormulaEdit = createControl(WS_EX_CLIENTEDGE, L"EDIT", L"weighted_score",
                                     ES_AUTOHSCROLL | WS_TABSTOP, 166, 187, 676, 30,
                                     IdScoreFormula, &settingsPage);
    createLabel(L"绩点排名", 56, 231, 110, 24, settingsPage);
    gpaFormulaEdit = createControl(WS_EX_CLIENTEDGE, L"EDIT", L"weighted_gpa",
                                   ES_AUTOHSCROLL | WS_TABSTOP, 166, 227, 676, 30,
                                   IdGpaFormula, &settingsPage);
    createLabel(L"综测排名", 56, 271, 110, 24, settingsPage);
    comprehensiveFormulaEdit = createControl(WS_EX_CLIENTEDGE, L"EDIT",
                                             L"weighted_score*0.8+bonus_total",
                                             ES_AUTOHSCROLL | WS_TABSTOP, 166, 267, 676, 30,
                                             IdComprehensiveFormula, &settingsPage);
    createLabel(L"支持 if、min、max、round 等函数；完整变量说明见“绩点说明”和操作手册。",
                166, 308, 676, 24, settingsPage);
    createControl(0, L"BUTTON", L"恢复默认公式", BS_PUSHBUTTON | WS_TABSTOP,
                  166, 334, 148, 34, IdResetFormulas, &settingsPage);

    createControl(0, L"BUTTON", L" 界面自定义 ", BS_GROUPBOX, 36, 394, 828, 142, 0, &settingsPage);
    createLabel(L"字体大小", 56, 426, 100, 24, settingsPage);
    fontSizeCombo = createControl(WS_EX_CLIENTEDGE, WC_COMBOBOXW, L"",
                                  CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                                  156, 421, 180, 120, IdFontSize, &settingsPage);
    SendMessageW(fontSizeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"紧凑（15）"));
    SendMessageW(fontSizeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"标准（17）"));
    SendMessageW(fontSizeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"较大（19）"));
    SendMessageW(fontSizeCombo, CB_SETCURSEL, interfaceFontSize == 15 ? 0 : (interfaceFontSize == 19 ? 2 : 1), 0);
    createLabel(L"界面主题", 368, 426, 100, 24, settingsPage);
    themeCombo = createControl(WS_EX_CLIENTEDGE, WC_COMBOBOXW, L"",
                               CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                               468, 421, 180, 120, IdTheme, &settingsPage);
    SendMessageW(themeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"标准灰"));
    SendMessageW(themeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"柔和蓝"));
    SendMessageW(themeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"柔和绿"));
    SendMessageW(themeCombo, CB_SETCURSEL, interfaceTheme, 0);
    createControl(0, L"BUTTON", L"应用界面设置", BS_PUSHBUTTON | WS_TABSTOP,
                  668, 419, 160, 34, IdApplyUi, &settingsPage);
    createLabel(L"设置会立即生效，并保存到程序目录下的 ui_settings.ini。",
                156, 476, 590, 24, settingsPage);

    createControl(0, L"BUTTON", L" 常用公式变量 ", BS_GROUPBOX, 36, 550, 828, 68, 0, &settingsPage);
    createLabel(L"weighted_score  加权成绩    weighted_gpa  平均绩点    bonus_total  综测加分",
                56, 576, 750, 25, settingsPage);

    const wchar_t* guide =
        L"成绩对应绩点（百分制）\r\n"
        L"90-100：4.0    85-89.5：3.7    82-84.5：3.3    78-81.5：3.0\r\n"
        L"75-77.5：2.7  72-74.5：2.3    68-71.5：2.0    64-67.5：1.5\r\n"
        L"60-63.5：1.0  低于60：0\r\n\r\n"
        L"五级制课程换算\r\n"
        L"优秀：课程分数95，绩点4.0    良好：课程分数85，绩点3.7\r\n"
        L"中等：课程分数75，绩点2.7    及格：课程分数65，绩点1.5\r\n"
        L"不及格：课程分数55，绩点0\r\n\r\n"
        L"两级制课程换算\r\n"
        L"合格：课程分数80，绩点3.0    不合格：课程分数50，绩点0\r\n\r\n"
        L"平均学分绩点（GPA）\r\n"
        L"GPA = Σ(课程绩点 × 课程学分) / Σ课程学分\r\n\r\n"
        L"加权平均成绩（WAN）\r\n"
        L"WAN = Σ(课程分数 × 课程学分) / Σ课程学分\r\n\r\n"
        L"程序中的默认变量\r\n"
        L"weighted_score：加权平均成绩    average_score：算术平均成绩\r\n"
        L"weighted_gpa：平均学分绩点      average_gpa：课程绩点算术平均\r\n"
        L"total_credits：总学分            course_count：课程数量\r\n"
        L"bonus_total：综测加分合计\r\n\r\n"
        L"默认综测公式：weighted_score*0.8+bonus_total\r\n"
        L"以上规则依据所提供的《成绩说明》图片整理。";
    HWND guideEdit = createControl(WS_EX_CLIENTEDGE, L"EDIT", guide,
                                    ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                                    36, 120, 828, 496, 0, &guidePage);
    SendMessageW(guideEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(14, 14));
    showPage(0);
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        mainWindow = window;
        createInterface();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IdBrowseInput: browseInput(); return 0;
        case IdBrowseOutput: browseOutput(); return 0;
        case IdCreateTemplate: createTemplate(); return 0;
        case IdCalculate: calculateRanking(); return 0;
        case IdOpenResult: openResult(); return 0;
        case IdResetFormulas: resetFormulas(); return 0;
        case IdApplyUi: applyUiCustomization(); return 0;
        case IdOpenManual: openManual(); return 0;
        default: break;
        }
        break;
    case WM_NOTIFY:
        if (reinterpret_cast<NMHDR*>(lParam)->idFrom == IdTab &&
            reinterpret_cast<NMHDR*>(lParam)->code == TCN_SELCHANGE) {
            showPage(TabCtrl_GetCurSel(tabControl));
            return 0;
        }
        break;
    case WM_ERASEBKGND: {
        RECT area{};
        GetClientRect(window, &area);
        FillRect(reinterpret_cast<HDC>(wParam), &area, backgroundBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC device = reinterpret_cast<HDC>(wParam);
        SetBkMode(device, TRANSPARENT);
        if (reinterpret_cast<HWND>(lParam) == titleLabel) SetTextColor(device, accentColor);
        return reinterpret_cast<LRESULT>(backgroundBrush);
    }
    case WM_CTLCOLORBTN:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(backgroundBrush);
    case WM_DESTROY:
        if (normalFont) DeleteObject(normalFont);
        if (titleFont) DeleteObject(titleFont);
        if (backgroundBrush) DeleteObject(backgroundBrush);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX commonControls{sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&commonControls);

    int argumentCount = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments) {
        for (int index = 1; index < argumentCount; ++index) {
            const std::wstring argument = arguments[index];
            if (argument == L"--input" && index + 1 < argumentCount) startupInput = arguments[++index];
            else if (argument == L"--output" && index + 1 < argumentCount) startupOutput = arguments[++index];
            else if (argument == L"--autorun") startupAutoRun = true;
        }
        LocalFree(arguments);
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = L"ClassRankerWindow";
    if (!RegisterClassExW(&windowClass)) return 1;

    RECT desired{0, 0, 900, 670};
    AdjustWindowRectEx(&desired, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                       FALSE, WS_EX_APPWINDOW);
    const int width = desired.right - desired.left;
    const int height = desired.bottom - desired.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    mainWindow = CreateWindowExW(WS_EX_APPWINDOW, windowClass.lpszClassName,
                                 L"ClassRanker - 班级排名计算",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                 x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!mainWindow) return 1;
    ShowWindow(mainWindow, showCommand);
    UpdateWindow(mainWindow);
    if (!startupInput.empty()) setText(inputEdit, startupInput);
    if (!startupOutput.empty()) setText(outputEdit, startupOutput);
    if (startupAutoRun) {
        PostMessageW(mainWindow, WM_COMMAND, MAKEWPARAM(IdCalculate, BN_CLICKED),
                     reinterpret_cast<LPARAM>(GetDlgItem(mainWindow, IdCalculate)));
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(mainWindow, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return static_cast<int>(message.wParam);
}

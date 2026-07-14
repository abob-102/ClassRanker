# ClassRanker

ClassRanker 是一个面向班级成绩统计的 Windows 桌面工具，用于计算成绩排名、绩点排名和综测排名。程序支持 `.xlsx` 与 CSV 输入，排名公式可自定义，适合班级、社团或课程小组进行成绩与综测数据整理。

## 功能特点

- Qt 图形界面，支持 Windows 桌面环境。
- 支持 `.xlsx` 工作簿和 CSV 目录输入。
- 输出成绩排名、绩点排名、综测排名和汇总表。
- 支持自定义课程绩点公式、成绩排名公式、绩点排名公式、综测排名公式。
- 支持示例 Excel 模板生成。
- 支持自定义 UI：
  - 字体大小
  - 主题颜色
  - 背景图片
  - GIF 动图背景
  - 背景透明度
  - 文字框透明度
- 提供命令行计算核心，便于调试和批处理。

## 程序结构

```text
ClassRanker/
├─ src/
│  ├─ main.cpp          # 命令行计算核心
│  ├─ qt_gui.cpp        # Qt 图形界面
│  ├─ csv.cpp           # CSV 读写
│  ├─ xlsx.cpp          # XLSX 读写
│  └─ expression.cpp    # 公式解析与计算
├─ config/
│  ├─ formulas.ini      # 默认公式配置
│  └─ ui_settings.ini   # 默认界面配置
├─ third_party/         # 第三方依赖
├─ class_data_template.xlsx
├─ 操作手册.txt
├─ CMakeLists.txt
└─ build_full.cmd
```

## 输入格式

`.xlsx` 文件需要包含三张工作表：

| 工作表 | 说明 |
|---|---|
| `Students` | 学生信息 |
| `Courses` | 课程成绩 |
| `Bonuses` | 综测加分 |

也可以使用 CSV 目录输入，目录中放置：

```text
students.csv
courses.csv
bonuses.csv
```

### Students

| 字段 | 含义 |
|---|---|
| `student_id` | 学号 |
| `name` | 姓名 |

### Courses

| 字段 | 含义 |
|---|---|
| `student_id` | 学号 |
| `course` | 课程名称 |
| `score` | 课程成绩 |
| `credit` | 学分 |
| `grade_point` | 课程绩点，可留空 |

### Bonuses

| 字段 | 含义 |
|---|---|
| `student_id` | 学号 |
| `item` | 加分项目 |
| `points` | 加分值 |

`Bonuses` 可以为空，但建议保留表头。

## 默认绩点规则

| 分数段 | 绩点 |
|---|---:|
| 90-100 | 4.0 |
| 85-89.5 | 3.7 |
| 82-84.5 | 3.3 |
| 78-81.5 | 3.0 |
| 75-77.5 | 2.7 |
| 72-74.5 | 2.3 |
| 68-71.5 | 2.0 |
| 64-67.5 | 1.5 |
| 60-63.5 | 1.0 |
| 60 以下 | 0 |

默认课程绩点公式：

```text
if(score>=90,4.0,if(score>=85,3.7,if(score>=82,3.3,if(score>=78,3.0,if(score>=75,2.7,if(score>=72,2.3,if(score>=68,2.0,if(score>=64,1.5,if(score>=60,1.0,0)))))))))
```

## 公式系统

课程绩点公式可使用变量：

```text
score
credit
grade_point
```

排名公式可使用变量：

```text
weighted_score
average_score
weighted_gpa
average_gpa
total_credits
course_count
bonus_total
```

支持运算符：

```text
+  -  *  /  %  ^
>  >=  <  <=  ==  !=
&&  ||  !
```

支持函数：

```text
if(condition, true_value, false_value)
min(a, b, ...)
max(a, b, ...)
abs(x)
sqrt(x)
round(x)
round(x, digits)
floor(x)
ceil(x)
pow(a, b)
clamp(x, min, max)
```

综测公式示例：

```text
weighted_score*0.8+bonus_total
```

## 图形界面使用

发布目录中：

```text
ClassRanker.exe
```

是 Qt 图形界面程序。

常用流程：

1. 双击 `ClassRanker.exe`。
2. 选择输入 Excel 文件或 CSV 目录。
3. 选择输出结果 `.xlsx` 路径。
4. 根据需要修改公式。
5. 点击“开始计算”。
6. 点击“打开结果”查看输出文件。

界面设置页支持：

- 修改公式
- 修改字体大小
- 修改主题
- 设置背景图片或 GIF
- 设置背景透明度
- 设置文字框透明度
- 保存界面设置

界面配置保存到：

```text
ui_settings.ini
```

## 命令行使用

生成 Excel 模板：

```powershell
ClassRankerCLI.exe --create-template class_data_template.xlsx
```

读取 Excel 并输出 Excel：

```powershell
ClassRankerCLI.exe --input class_data_template.xlsx --output ranking_results.xlsx --config formulas.ini --no-prompt
```

读取 CSV 目录并输出 CSV 目录：

```powershell
ClassRankerCLI.exe --input examples --output ranking_results --config formulas.ini --no-prompt
```

## 构建环境

推荐环境：

- Windows 10/11
- Visual Studio 2022
- CMake 3.20+
- Qt 6，MSVC 2022 64-bit

本项目使用 OpenXLSX 读写 `.xlsx` 文件，源码包位于 `third_party/`。

### 构建

使用项目自带脚本：

```powershell
.\build_full.cmd
```

或手动构建：

```powershell
cmake -S . -B build-full-ok -G "Visual Studio 17 2022" -A x64 -DCLASSRANKER_WITH_XLSX=ON -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64
cmake --build build-full-ok --config Release
```

构建结果位于：

```text
build-full-ok/Release
```

## 打包发布

Qt 版本发布时，需要把 Qt 运行库一起复制到发布目录。可使用：

```powershell
windeployqt --release --compiler-runtime ClassRanker.exe
```

发布目录至少需要包含：

```text
ClassRanker.exe
ClassRankerCLI.exe
class_data_template.xlsx
formulas.ini
ui_settings.ini
操作手册.txt
Qt6Core.dll
Qt6Gui.dll
Qt6Widgets.dll
platforms/qwindows.dll
```

如果要在未安装 Visual Studio 的电脑运行，还需要包含 MSVC 运行库，例如：

```text
msvcp140.dll
vcruntime140.dll
vcruntime140_1.dll
```

## 文件格式限制

当前支持：

- `.xlsx`
- CSV

当前不支持：

- `.xls`

`.xls` 是老版 Excel 二进制格式，当前使用的 OpenXLSX 不支持。请先用 Excel 或 WPS 将 `.xls` 另存为 `.xlsx` 后再导入。

## 注意事项

- 输出文件如果正在被 Excel 打开，程序可能无法覆盖写入。
- 自定义背景图片路径保存在 `ui_settings.ini` 中，跨电脑使用时需要重新选择背景文件，或保证背景文件路径一致。
- 公式变量和函数名必须使用英文。
- 修改源码后需要重新编译并重新部署 Qt 运行库。

## License

请根据实际项目要求补充许可证，例如 MIT、Apache-2.0 或内部使用说明。

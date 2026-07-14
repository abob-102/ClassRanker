#include <QtWidgets>

namespace {

const QString kDefaultCourseFormula =
    "if(score>=90,4.0,if(score>=85,3.7,if(score>=82,3.3,if(score>=78,3.0,"
    "if(score>=75,2.7,if(score>=72,2.3,if(score>=68,2.0,if(score>=64,1.5,if(score>=60,1.0,0)))))))))";
const QString kDefaultScoreFormula = "weighted_score";
const QString kDefaultGpaFormula = "weighted_gpa";
const QString kDefaultComprehensiveFormula = "weighted_score*0.8+bonus_total";

QString appDir() {
    return QCoreApplication::applicationDirPath();
}

QString backendPath() {
    const QString deployed = QDir(appDir()).filePath("ClassRankerCLI.exe");
    if (QFileInfo::exists(deployed)) return deployed;
    return QDir(appDir()).filePath("ClassRanker.exe");
}

QLineEdit* makePathEdit(const QString& text) {
    auto* edit = new QLineEdit(text);
    edit->setMinimumHeight(42);
    edit->setClearButtonEnabled(true);
    return edit;
}

QPlainTextEdit* makeFormulaEdit(const QString& value) {
    auto* edit = new QPlainTextEdit(value);
    edit->setMinimumHeight(82);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    return edit;
}

QLabel* makeTitle(const QString& text) {
    auto* label = new QLabel(text);
    QFont font = label->font();
    font.setPointSize(16);
    font.setBold(true);
    label->setFont(font);
    return label;
}

QTableWidget* makeGradeTable(const QStringList& headers, const QList<QStringList>& rows) {
    auto* table = new QTableWidget(rows.size(), headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setMinimumSectionSize(96);
    table->setMinimumHeight(260);
    for (int row = 0; row < rows.size(); ++row) {
        for (int col = 0; col < rows[row].size(); ++col) {
            auto* item = new QTableWidgetItem(rows[row][col]);
            item->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col, item);
        }
    }
    return table;
}

QString processText(const QByteArray& output) {
    QString text = QString::fromUtf8(output);
    if (text.trimmed().isEmpty()) text = QString::fromLocal8Bit(output);
    return text.trimmed();
}

void openPath(QWidget* parent, const QString& path) {
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        QMessageBox::warning(parent, "无法打开", "路径不存在。");
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("班级成绩绩点综测排名工具");
        resize(1120, 780);
        setMinimumSize(980, 680);
        setAutoFillBackground(false);
        buildUi();
        loadSettings();
        applyTheme();
    }

    ~MainWindow() override {
        delete backgroundMovie_;
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.fillRect(rect(), themeBaseColor());

        QPixmap image;
        if (backgroundMovie_ && backgroundMovie_->isValid()) {
            image = backgroundMovie_->currentPixmap();
        } else {
            image = backgroundPixmap_;
        }
        if (image.isNull() || backgroundOpacitySlider_ == nullptr || backgroundOpacitySlider_->value() <= 0) return;

        painter.setOpacity(backgroundOpacitySlider_->value() / 100.0);
        const int mode = backgroundModeBox_ ? backgroundModeBox_->currentIndex() : 0;
        if (mode == 3) {
            painter.drawTiledPixmap(rect(), image);
            return;
        }

        QSize targetSize = image.size();
        if (mode == 0) {
            targetSize.scale(size(), Qt::KeepAspectRatioByExpanding);
        } else if (mode == 1) {
            targetSize.scale(size(), Qt::KeepAspectRatio);
        }
        const QPixmap scaled = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QPoint topLeft((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
        painter.drawPixmap(topLeft, scaled);
    }

    void closeEvent(QCloseEvent* event) override {
        saveSettings();
        QMainWindow::closeEvent(event);
    }

private:
    QLineEdit* inputEdit_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QPlainTextEdit* courseFormula_ = nullptr;
    QPlainTextEdit* scoreFormula_ = nullptr;
    QPlainTextEdit* gpaFormula_ = nullptr;
    QPlainTextEdit* comprehensiveFormula_ = nullptr;
    QTextEdit* status_ = nullptr;
    QPushButton* openResultButton_ = nullptr;
    QComboBox* fontSizeBox_ = nullptr;
    QComboBox* themeBox_ = nullptr;
    QLineEdit* backgroundEdit_ = nullptr;
    QComboBox* backgroundModeBox_ = nullptr;
    QSlider* backgroundOpacitySlider_ = nullptr;
    QSlider* editorOpacitySlider_ = nullptr;
    QMovie* backgroundMovie_ = nullptr;
    QPixmap backgroundPixmap_;
    QString backgroundPath_;
    QString lastOutput_;
    bool loadingSettings_ = false;

    QColor themeBaseColor() const {
        if (themeBox_ && themeBox_->currentIndex() == 1) return QColor("#eef6fc");
        if (themeBox_ && themeBox_->currentIndex() == 2) return QColor("#f1f8f4");
        return QColor("#f7f9fc");
    }

    void buildUi() {
        auto* tabs = new QTabWidget;
        tabs->addTab(buildCalculatePage(), "排名计算");
        tabs->addTab(buildFormulaPage(), "公式与界面");
        tabs->addTab(buildGpaGuidePage(), "绩点说明");
        setCentralWidget(tabs);
    }

    QWidget* buildCalculatePage() {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(14);

        inputEdit_ = makePathEdit(QDir(appDir()).filePath("class_data_template.xlsx"));
        outputEdit_ = makePathEdit(QDir(appDir()).filePath("ranking_results.xlsx"));

        auto* form = new QFormLayout;
        form->setLabelAlignment(Qt::AlignRight);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->addRow("输入 Excel/CSV 目录",
                     makePathRow(inputEdit_, [this] { browseInput(); }, [this] { openPath(this, inputEdit_->text().trimmed()); }));
        form->addRow("输出结果路径",
                     makePathRow(outputEdit_, [this] { browseOutput(); }, [this] { openPath(this, outputEdit_->text().trimmed()); }));
        layout->addLayout(form);

        auto* buttons = new QHBoxLayout;
        auto* templateButton = new QPushButton(style()->standardIcon(QStyle::SP_FileIcon), "生成示例表格");
        auto* runButton = new QPushButton(style()->standardIcon(QStyle::SP_MediaPlay), "开始计算");
        openResultButton_ = new QPushButton(style()->standardIcon(QStyle::SP_DirOpenIcon), "打开结果");
        auto* manualButton = new QPushButton(style()->standardIcon(QStyle::SP_FileDialogInfoView), "打开操作手册");
        openResultButton_->setEnabled(false);
        buttons->addWidget(templateButton);
        buttons->addWidget(runButton);
        buttons->addWidget(openResultButton_);
        buttons->addStretch();
        buttons->addWidget(manualButton);
        layout->addLayout(buttons);

        status_ = new QTextEdit;
        status_->setReadOnly(true);
        status_->setMinimumHeight(300);
        status_->setText("请选择输入表格和输出路径，然后开始计算。");
        layout->addWidget(status_, 1);

        connect(templateButton, &QPushButton::clicked, this, &MainWindow::createTemplate);
        connect(runButton, &QPushButton::clicked, this, &MainWindow::calculate);
        connect(openResultButton_, &QPushButton::clicked, this, [this] { openPath(this, lastOutput_); });
        connect(manualButton, &QPushButton::clicked, this, [this] {
            openPath(this, QDir(appDir()).filePath("操作手册.txt"));
        });
        return page;
    }

    QWidget* makePathRow(QLineEdit* edit, const std::function<void()>& browse, const std::function<void()>& open) {
        auto* widget = new QWidget;
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* button = new QPushButton(style()->standardIcon(QStyle::SP_DirOpenIcon), "");
        button->setToolTip("浏览");
        button->setFixedSize(50, 42);
        auto* openButton = new QPushButton(style()->standardIcon(QStyle::SP_FileDialogContentsView), "");
        openButton->setToolTip("打开文件");
        openButton->setFixedSize(50, 42);
        layout->addWidget(edit, 1);
        layout->addWidget(button);
        layout->addWidget(openButton);
        connect(button, &QPushButton::clicked, this, browse);
        connect(openButton, &QPushButton::clicked, this, open);
        return widget;
    }

    QWidget* buildFormulaPage() {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(12);

        courseFormula_ = makeFormulaEdit(kDefaultCourseFormula);
        scoreFormula_ = makeFormulaEdit(kDefaultScoreFormula);
        gpaFormula_ = makeFormulaEdit(kDefaultGpaFormula);
        comprehensiveFormula_ = makeFormulaEdit(kDefaultComprehensiveFormula);

        auto* form = new QFormLayout;
        form->setLabelAlignment(Qt::AlignRight);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->addRow("课程绩点公式", courseFormula_);
        form->addRow("成绩排名公式", scoreFormula_);
        form->addRow("绩点排名公式", gpaFormula_);
        form->addRow("综测排名公式", comprehensiveFormula_);
        layout->addLayout(form, 1);

        auto* uiRow = new QHBoxLayout;
        fontSizeBox_ = new QComboBox;
        fontSizeBox_->addItems({"小", "标准", "大"});
        themeBox_ = new QComboBox;
        themeBox_->addItems({"浅灰", "柔和蓝", "柔和绿"});
        auto* resetButton = new QPushButton(style()->standardIcon(QStyle::SP_BrowserReload), "恢复默认公式");
        auto* saveUiButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton), "保存界面设置");
        uiRow->addWidget(new QLabel("字体"));
        uiRow->addWidget(fontSizeBox_);
        uiRow->addSpacing(20);
        uiRow->addWidget(new QLabel("主题"));
        uiRow->addWidget(themeBox_);
        uiRow->addStretch();
        uiRow->addWidget(saveUiButton);
        uiRow->addWidget(resetButton);
        layout->addLayout(uiRow);

        auto* backgroundGroup = new QGroupBox("界面背景");
        auto* backgroundLayout = new QGridLayout(backgroundGroup);
        backgroundEdit_ = makePathEdit("");
        auto* browseBackgroundButton = new QPushButton(style()->standardIcon(QStyle::SP_DirOpenIcon), "选择图片/GIF");
        auto* clearBackgroundButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogResetButton), "清除背景");
        backgroundModeBox_ = new QComboBox;
        backgroundModeBox_->addItems({"填充", "适应", "居中", "平铺"});
        backgroundOpacitySlider_ = new QSlider(Qt::Horizontal);
        backgroundOpacitySlider_->setRange(0, 100);
        backgroundOpacitySlider_->setValue(65);
        backgroundOpacitySlider_->setTickInterval(10);
        backgroundOpacitySlider_->setTickPosition(QSlider::TicksBelow);
        editorOpacitySlider_ = new QSlider(Qt::Horizontal);
        editorOpacitySlider_->setRange(35, 100);
        editorOpacitySlider_->setValue(82);
        editorOpacitySlider_->setTickInterval(10);
        editorOpacitySlider_->setTickPosition(QSlider::TicksBelow);
        backgroundLayout->addWidget(new QLabel("文件"), 0, 0);
        backgroundLayout->addWidget(backgroundEdit_, 0, 1);
        backgroundLayout->addWidget(browseBackgroundButton, 0, 2);
        backgroundLayout->addWidget(clearBackgroundButton, 0, 3);
        backgroundLayout->addWidget(new QLabel("模式"), 1, 0);
        backgroundLayout->addWidget(backgroundModeBox_, 1, 1);
        backgroundLayout->addWidget(new QLabel("透明度"), 1, 2);
        backgroundLayout->addWidget(backgroundOpacitySlider_, 1, 3);
        backgroundLayout->addWidget(new QLabel("文字框透明度"), 2, 0);
        backgroundLayout->addWidget(editorOpacitySlider_, 2, 1, 1, 3);
        backgroundLayout->setColumnStretch(1, 1);
        layout->addWidget(backgroundGroup);

        connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetFormulas);
        connect(saveUiButton, &QPushButton::clicked, this, &MainWindow::saveSettingsWithNotice);
        connect(browseBackgroundButton, &QPushButton::clicked, this, &MainWindow::browseBackground);
        connect(clearBackgroundButton, &QPushButton::clicked, this, &MainWindow::clearBackground);
        connect(backgroundModeBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
            saveSettings();
            update();
        });
        connect(backgroundOpacitySlider_, &QSlider::valueChanged, this, [this] {
            saveSettings();
            update();
        });
        connect(editorOpacitySlider_, &QSlider::valueChanged, this, [this] {
            applyTheme();
            saveSettings();
        });
        connect(backgroundEdit_, &QLineEdit::editingFinished, this, [this] {
            setBackgroundPath(backgroundEdit_->text().trimmed());
            saveSettings();
        });
        connect(fontSizeBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
            applyTheme();
            saveSettings();
        });
        connect(themeBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
            applyTheme();
            saveSettings();
        });
        return page;
    }

    QWidget* buildGpaGuidePage() {
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        auto* content = new QWidget;
        auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(14);

        layout->addWidget(makeTitle("百分制成绩对应绩点"));
        layout->addWidget(makeGradeTable({"分数段", "课程绩点"}, {
            {"90-100", "4.0"}, {"85-89.5", "3.7"}, {"82-84.5", "3.3"},
            {"78-81.5", "3.0"}, {"75-77.5", "2.7"}, {"72-74.5", "2.3"},
            {"68-71.5", "2.0"}, {"64-67.5", "1.5"}, {"60-63.5", "1.0"}, {"60 以下", "0"}
        }));

        layout->addWidget(makeTitle("平均绩点与平均成绩公式"));
        auto* formulaBox = new QTextEdit;
        formulaBox->setReadOnly(true);
        formulaBox->setMinimumHeight(120);
        formulaBox->setText("平均绩点 GPA = Σ(课程绩点 × 课程学分) / Σ课程学分\n"
                            "加权平均分 WAN = Σ(课程分数 × 课程学分) / Σ课程学分\n\n"
                            "程序默认使用上表规则计算课程绩点；如果 Excel 中填写了 grade_point，也可以在公式中直接使用。");
        layout->addWidget(formulaBox);

        layout->addWidget(makeTitle("公式变量"));
        layout->addWidget(makeGradeTable({"变量名", "含义"}, {
            {"score", "单门课程成绩，课程绩点公式使用"},
            {"credit", "单门课程学分，课程绩点公式使用"},
            {"grade_point", "Excel 中填写的课程绩点"},
            {"weighted_score", "加权平均分"},
            {"average_score", "算术平均分"},
            {"weighted_gpa", "加权平均绩点"},
            {"average_gpa", "算术平均绩点"},
            {"bonus_total", "综测加分合计"},
            {"total_credits", "总学分"},
            {"course_count", "课程数量"}
        }));

        layout->addStretch();
        scroll->setWidget(content);
        return scroll;
    }

    void browseInput() {
        const QString file = QFileDialog::getOpenFileName(this, "选择 Excel 文件", appDir(), "Excel (*.xlsx);;所有文件 (*.*)");
        if (!file.isEmpty()) inputEdit_->setText(file);
    }

    void browseOutput() {
        const QString file = QFileDialog::getSaveFileName(this, "选择输出文件", outputEdit_->text(), "Excel (*.xlsx);;CSV 目录名 (*)");
        if (!file.isEmpty()) outputEdit_->setText(file);
    }

    void browseBackground() {
        const QString file = QFileDialog::getOpenFileName(
            this, "选择背景图片或 GIF", appDir(),
            "图片和动图 (*.png *.jpg *.jpeg *.bmp *.gif);;所有文件 (*.*)");
        if (file.isEmpty()) return;
        backgroundEdit_->setText(file);
        setBackgroundPath(file);
        saveSettings();
    }

    void clearBackground() {
        backgroundEdit_->clear();
        setBackgroundPath("");
        saveSettings();
    }

    void setBackgroundPath(const QString& path) {
        backgroundPath_ = path;
        backgroundPixmap_ = QPixmap();
        delete backgroundMovie_;
        backgroundMovie_ = nullptr;

        if (path.isEmpty() || !QFileInfo::exists(path)) {
            update();
            return;
        }

        if (QFileInfo(path).suffix().compare("gif", Qt::CaseInsensitive) == 0) {
            backgroundMovie_ = new QMovie(path);
            backgroundMovie_->setCacheMode(QMovie::CacheAll);
            connect(backgroundMovie_, &QMovie::frameChanged, this, [this] { update(); });
            backgroundMovie_->start();
        } else {
            backgroundPixmap_.load(path);
        }
        update();
    }

    bool writeFormulaFile(QString* path) {
        QTemporaryFile file(QDir::tempPath() + "/classranker-formulas-XXXXXX.ini");
        file.setAutoRemove(false);
        if (!file.open()) {
            QMessageBox::critical(this, "公式保存失败", "无法创建临时公式配置文件。");
            return false;
        }
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << "course_gpa_formula=" << courseFormula_->toPlainText().trimmed() << "\n";
        out << "score_ranking_formula=" << scoreFormula_->toPlainText().trimmed() << "\n";
        out << "gpa_ranking_formula=" << gpaFormula_->toPlainText().trimmed() << "\n";
        out << "comprehensive_ranking_formula=" << comprehensiveFormula_->toPlainText().trimmed() << "\n";
        *path = file.fileName();
        return true;
    }

    int runBackend(const QStringList& arguments, QString* text) {
        QProcess process;
        process.setProgram(backendPath());
        process.setArguments(arguments);
        process.setWorkingDirectory(appDir());
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start();
        if (!process.waitForStarted()) {
            *text = "无法启动后台计算程序 ClassRankerCLI.exe。";
            return -1;
        }
        process.waitForFinished(-1);
        *text = processText(process.readAll());
        return process.exitCode();
    }

    void createTemplate() {
        const QString target = inputEdit_->text().trimmed();
        if (target.isEmpty()) {
            QMessageBox::warning(this, "缺少路径", "请先填写示例表格保存路径。");
            return;
        }
        QString text;
        const int code = runBackend({"--create-template", target}, &text);
        status_->setText(text);
        if (code == 0) QMessageBox::information(this, "生成完成", "示例表格已生成。");
    }

    void calculate() {
        const QString input = inputEdit_->text().trimmed();
        const QString output = outputEdit_->text().trimmed();
        if (input.isEmpty() || output.isEmpty()) {
            QMessageBox::warning(this, "路径不完整", "请填写输入路径和输出路径。");
            return;
        }
        QString configPath;
        if (!writeFormulaFile(&configPath)) return;
        QString text;
        const int code = runBackend({"--input", input, "--output", output, "--config", configPath, "--no-prompt"}, &text);
        QFile::remove(configPath);
        status_->setText(text);
        if (code == 0) {
            lastOutput_ = output;
            openResultButton_->setEnabled(true);
            saveSettings();
            QMessageBox::information(this, "计算完成", "排名结果已保存。");
        } else {
            QMessageBox::critical(this, "计算失败", text.isEmpty() ? "后台程序返回错误。" : text);
        }
    }

    void resetFormulas() {
        courseFormula_->setPlainText(kDefaultCourseFormula);
        scoreFormula_->setPlainText(kDefaultScoreFormula);
        gpaFormula_->setPlainText(kDefaultGpaFormula);
        comprehensiveFormula_->setPlainText(kDefaultComprehensiveFormula);
        saveSettings();
    }

    void saveSettingsWithNotice() {
        saveSettings();
        QMessageBox::information(this, "保存完成",
                                 "界面设置已保存到：\n" + QDir(appDir()).filePath("ui_settings.ini"));
    }

    void loadSettings() {
        loadingSettings_ = true;
        QSettings settings(QDir(appDir()).filePath("ui_settings.ini"), QSettings::IniFormat);
        inputEdit_->setText(settings.value("paths/input", inputEdit_->text()).toString());
        outputEdit_->setText(settings.value("paths/output", outputEdit_->text()).toString());
        courseFormula_->setPlainText(settings.value("formulas/course", kDefaultCourseFormula).toString());
        scoreFormula_->setPlainText(settings.value("formulas/score", kDefaultScoreFormula).toString());
        gpaFormula_->setPlainText(settings.value("formulas/gpa", kDefaultGpaFormula).toString());
        comprehensiveFormula_->setPlainText(settings.value("formulas/comprehensive", kDefaultComprehensiveFormula).toString());
        fontSizeBox_->setCurrentIndex(settings.value("ui/font_size_index", 1).toInt());
        themeBox_->setCurrentIndex(settings.value("ui/theme", 0).toInt());
        backgroundEdit_->setText(settings.value("background/path", "").toString());
        backgroundModeBox_->setCurrentIndex(settings.value("background/mode", 0).toInt());
        backgroundOpacitySlider_->setValue(settings.value("background/opacity", 65).toInt());
        editorOpacitySlider_->setValue(settings.value("background/editor_opacity", 82).toInt());
        setBackgroundPath(backgroundEdit_->text().trimmed());
        loadingSettings_ = false;
        applyTheme();
        update();
    }

    void saveSettings() {
        if (loadingSettings_) return;
        QSettings settings(QDir(appDir()).filePath("ui_settings.ini"), QSettings::IniFormat);
        settings.setValue("paths/input", inputEdit_->text());
        settings.setValue("paths/output", outputEdit_->text());
        settings.setValue("formulas/course", courseFormula_->toPlainText());
        settings.setValue("formulas/score", scoreFormula_->toPlainText());
        settings.setValue("formulas/gpa", gpaFormula_->toPlainText());
        settings.setValue("formulas/comprehensive", comprehensiveFormula_->toPlainText());
        settings.setValue("ui/font_size_index", fontSizeBox_->currentIndex());
        settings.setValue("ui/theme", themeBox_->currentIndex());
        settings.setValue("background/path", backgroundEdit_->text());
        settings.setValue("background/mode", backgroundModeBox_->currentIndex());
        settings.setValue("background/opacity", backgroundOpacitySlider_->value());
        settings.setValue("background/editor_opacity", editorOpacitySlider_->value());
        settings.sync();
    }

    void applyTheme() {
        const int size = fontSizeBox_->currentIndex() == 0 ? 11 : (fontSizeBox_->currentIndex() == 2 ? 15 : 13);
        QFont font("Microsoft YaHei UI", size);
        qApp->setFont(font);

        QString accent = "#2563eb";
        QString bg = "#f7f9fc";
        if (themeBox_->currentIndex() == 1) {
            accent = "#0f6694";
            bg = "#eef6fc";
        } else if (themeBox_->currentIndex() == 2) {
            accent = "#28724f";
            bg = "#f1f8f4";
        }
        const int editorAlpha = editorOpacitySlider_ ? qBound(35, editorOpacitySlider_->value(), 100) * 255 / 100 : 209;
        qApp->setStyleSheet(QString(R"(
            QMainWindow { background: %1; color: #111827; }
            QWidget { background: transparent; color: #111827; }
            QLineEdit, QPlainTextEdit, QTextEdit, QTableWidget {
                background: rgba(255,255,255,%3); color: #111827; border: 1px solid #9ca3af; border-radius: 6px; padding: 8px;
                selection-background-color: %2; selection-color: white;
            }
            QGroupBox {
                background: rgba(255,255,255,150); color: #111827; border: 1px solid #cbd5e1;
                border-radius: 6px; margin-top: 12px; padding: 12px;
            }
            QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
            QPushButton {
                background: white; color: #111827; border: 1px solid #8b98a8; border-radius: 6px; padding: 10px 16px;
                min-height: 24px;
            }
            QPushButton:hover { border-color: %2; color: %2; }
            QPushButton:disabled { color: #94a3b8; background: #f1f5f9; }
            QTabWidget::pane { border: 1px solid #cbd5e1; background: rgba(255,255,255,72); }
            QTabBar::tab { padding: 12px 24px; background: rgba(231,237,245,190); border: 1px solid #cbd5e1; color: #111827; }
            QTabBar::tab:selected { background: white; color: %2; }
            QLabel { color: #111827; }
            QComboBox { background: rgba(255,255,255,%3); color: #111827; min-height: 36px; padding: 4px 8px; }
            QHeaderView::section { background: #e9eef5; color: #111827; padding: 10px; border: 0; }
        )").arg(bg, accent).arg(editorAlpha));
    }
};

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("ClassRanker");
    QApplication::setApplicationName("ClassRanker");
    MainWindow window;
    window.show();
    return app.exec();
}

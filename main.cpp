#include "codeeditor.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTextStream>
#include <QTextCharFormat>
#include <QTimer>
#include <QToolBar>

class MainWindow : public QMainWindow
{
public:
    MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    CodeEditor *editor;
    QString currentFile;
    QLabel *positionLabel;
    QLabel *stateLabel;
    QTimer *autoSaveTimer;
    QAction *saveAction;
    QAction *undoAction;
    QAction *redoAction;
    QAction *cutAction;
    QAction *copyAction;
    QAction *pasteAction;

    QAction *makeAction(const QString &text, const QKeySequence &shortcut);
    void createActions();
    void createMenusAndToolBars();
    void updateTitle();
    void updateCursorInfo();
    void updateEditActions();
    bool maybeSave();
    bool saveFile(const QString &fileName);
    void newFile();
    void openFile();
    void save();
    void saveAs();
    void showAbout();
    void showFindDialog();
    void showReplaceDialog();
    void setFontForSelection();
    void setTextColor();
    void setBackgroundColor();
    void toggleWrap(bool checked);
    void toggleAutoSave(bool checked);
};

MainWindow::MainWindow()
    : editor(new CodeEditor(this)), positionLabel(new QLabel(this)), stateLabel(new QLabel(this)),
      autoSaveTimer(new QTimer(this))
{
    setCentralWidget(editor);
    resize(980, 680);
    setMinimumSize(680, 420);
    editor->setPlaceholderText("在这里输入文本...");
    createActions();
    createMenusAndToolBars();

    statusBar()->addPermanentWidget(positionLabel);
    statusBar()->addPermanentWidget(stateLabel);
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this](bool) { updateTitle(); });
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this] { updateCursorInfo(); });
    connect(editor, &QPlainTextEdit::copyAvailable, this, [this](bool) { updateEditActions(); });
    connect(editor->document(), &QTextDocument::undoAvailable, undoAction, &QAction::setEnabled);
    connect(editor->document(), &QTextDocument::redoAvailable, redoAction, &QAction::setEnabled);
    connect(autoSaveTimer, &QTimer::timeout, this, [this] {
        if (editor->document()->isModified() && !currentFile.isEmpty()) save();
    });
    updateEditActions();
    updateCursorInfo();
    updateTitle();
}

QAction *MainWindow::makeAction(const QString &text, const QKeySequence &shortcut)
{
    QAction *action = new QAction(text, this);
    action->setShortcut(shortcut);
    return action;
}

void MainWindow::createActions()
{
    saveAction = makeAction("保存(&S)", QKeySequence::Save);
    undoAction = makeAction("撤销(&U)", QKeySequence::Undo);
    redoAction = makeAction("重做(&R)", QKeySequence::Redo);
    cutAction = makeAction("剪切(&T)", QKeySequence::Cut);
    copyAction = makeAction("复制(&C)", QKeySequence::Copy);
    pasteAction = makeAction("粘贴(&P)", QKeySequence::Paste);
    connect(saveAction, &QAction::triggered, this, &MainWindow::save);
    connect(undoAction, &QAction::triggered, editor, &QPlainTextEdit::undo);
    connect(redoAction, &QAction::triggered, editor, &QPlainTextEdit::redo);
    connect(cutAction, &QAction::triggered, editor, &QPlainTextEdit::cut);
    connect(copyAction, &QAction::triggered, editor, &QPlainTextEdit::copy);
    connect(pasteAction, &QAction::triggered, editor, &QPlainTextEdit::paste);
}

void MainWindow::createMenusAndToolBars()
{
    QMenu *fileMenu = menuBar()->addMenu("文件(&F)");
    QAction *newAction = makeAction("新建(&N)", QKeySequence::New);
    QAction *openAction = makeAction("打开(&O)", QKeySequence::Open);
    QAction *saveAsAction = makeAction("另存为(&A)", QKeySequence::SaveAs);
    connect(newAction, &QAction::triggered, this, &MainWindow::newFile);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveAs);
    fileMenu->addActions({newAction, openAction, saveAction, saveAsAction});
    fileMenu->addSeparator();
    fileMenu->addAction("退出(&X)", this, &QWidget::close, QKeySequence::Quit);

    QMenu *editMenu = menuBar()->addMenu("编辑(&E)");
    editMenu->addActions({undoAction, redoAction});
    editMenu->addSeparator();
    editMenu->addActions({cutAction, copyAction, pasteAction});
    editMenu->addAction("全选(&A)", editor, &QPlainTextEdit::selectAll, QKeySequence::SelectAll);
    editMenu->addSeparator();
    editMenu->addAction("查找(&F)", this, &MainWindow::showFindDialog, QKeySequence::Find);
    editMenu->addAction("替换(&R)", this, &MainWindow::showReplaceDialog, QKeySequence::Replace);

    QMenu *formatMenu = menuBar()->addMenu("格式(&O)");
    formatMenu->addAction("字体(&F)...", this, &MainWindow::setFontForSelection);
    formatMenu->addAction("文字颜色(&C)...", this, &MainWindow::setTextColor);
    formatMenu->addAction("背景颜色(&B)...", this, &MainWindow::setBackgroundColor);
    QAction *wrapAction = formatMenu->addAction("自动换行");
    wrapAction->setCheckable(true);
    connect(wrapAction, &QAction::toggled, this, &MainWindow::toggleWrap);
    QAction *autoSaveAction = formatMenu->addAction("自动保存（30秒）");
    autoSaveAction->setCheckable(true);
    connect(autoSaveAction, &QAction::toggled, this, &MainWindow::toggleAutoSave);

    QMenu *helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("关于(&A)", this, &MainWindow::showAbout);

    QToolBar *toolBar = addToolBar("常用工具");
    toolBar->addActions({newAction, openAction, saveAction});
    toolBar->addSeparator();
    toolBar->addActions({cutAction, copyAction, pasteAction});
}

void MainWindow::updateTitle()
{
    const QString name = currentFile.isEmpty() ? "未命名.txt" : QFileInfo(currentFile).fileName();
    setWindowTitle(QString("%1%2 - 简单文本编辑器").arg(name, editor->document()->isModified() ? " *" : ""));
    stateLabel->setText(editor->document()->isModified() ? "已修改" : "已保存");
}

void MainWindow::updateCursorInfo()
{
    const QTextCursor cursor = editor->textCursor();
    positionLabel->setText(QString("第 %1 行，第 %2 列").arg(cursor.blockNumber() + 1).arg(cursor.columnNumber() + 1));
}

void MainWindow::updateEditActions()
{
    const bool hasSelection = editor->textCursor().hasSelection();
    cutAction->setEnabled(hasSelection);
    copyAction->setEnabled(hasSelection);
}

bool MainWindow::maybeSave()
{
    if (!editor->document()->isModified()) return true;
    const QMessageBox::StandardButton result = QMessageBox::warning(
        this, "未保存的修改", "当前文件已修改，是否保存？",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (result == QMessageBox::Save) { save(); return !editor->document()->isModified(); }
    return result == QMessageBox::Discard;
}

bool MainWindow::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "保存失败", "无法写入文件：" + file.errorString());
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << editor->toPlainText();
    currentFile = fileName;
    editor->document()->setModified(false);
    updateTitle();
    statusBar()->showMessage("文件已保存", 2500);
    return true;
}

void MainWindow::newFile()
{
    if (!maybeSave()) return;
    editor->clear();
    currentFile.clear();
    editor->document()->setModified(false);
    updateTitle();
}

void MainWindow::openFile()
{
    if (!maybeSave()) return;
    const QString fileName = QFileDialog::getOpenFileName(this, "打开文本文件", QString(), "文本文件 (*.txt);;所有文件 (*.*)");
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "打开失败", "无法读取文件：" + file.errorString());
        return;
    }
    QTextStream in(&file);
    in.setCodec("UTF-8");
    editor->setPlainText(in.readAll());
    currentFile = fileName;
    editor->document()->setModified(false);
    updateTitle();
    statusBar()->showMessage("文件已打开", 2500);
}

void MainWindow::save()
{
    if (currentFile.isEmpty()) saveAs();
    else saveFile(currentFile);
}

void MainWindow::saveAs()
{
    const QString fileName = QFileDialog::getSaveFileName(this, "另存为", currentFile.isEmpty() ? "未命名.txt" : currentFile, "文本文件 (*.txt);;所有文件 (*.*)");
    if (!fileName.isEmpty()) saveFile(fileName);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "关于", "简单文本编辑器\nQt 5.15.2 Widgets\n支持文件编辑、查找替换、样式设置与行号显示。");
}

void MainWindow::showFindDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("查找");
    QFormLayout layout(&dialog);
    QLineEdit textEdit;
    QCheckBox caseBox("区分大小写");
    QDialogButtonBox buttons(QDialogButtonBox::Close);
    layout.addRow("查找内容：", &textEdit);
    layout.addRow(&caseBox);
    QPushButton *nextButton = new QPushButton("查找下一个");
    layout.addRow(nextButton);
    layout.addRow(&buttons);
    connect(nextButton, &QPushButton::clicked, &dialog, [this, &textEdit, &caseBox] {
        if (textEdit.text().isEmpty()) return;
        QTextDocument::FindFlags flags = caseBox.isChecked() ? QTextDocument::FindCaseSensitively : QTextDocument::FindFlags();
        if (!editor->find(textEdit.text(), flags)) QMessageBox::information(this, "查找", "未找到匹配内容。");
    });
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void MainWindow::showReplaceDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("替换");
    QFormLayout layout(&dialog);
    QLineEdit findEdit, replaceEdit;
    QDialogButtonBox buttons(QDialogButtonBox::Close);
    layout.addRow("查找内容：", &findEdit);
    layout.addRow("替换为：", &replaceEdit);
    QPushButton *replaceButton = new QPushButton("替换");
    QPushButton *allButton = new QPushButton("全部替换");
    layout.addRow(replaceButton, allButton);
    layout.addRow(&buttons);
    connect(replaceButton, &QPushButton::clicked, &dialog, [this, &findEdit, &replaceEdit] {
        if (findEdit.text().isEmpty()) return;
        if (!editor->textCursor().hasSelection() || editor->textCursor().selectedText() != findEdit.text()) editor->find(findEdit.text());
        if (editor->textCursor().hasSelection()) editor->textCursor().insertText(replaceEdit.text());
        else QMessageBox::information(this, "替换", "未找到匹配内容。");
    });
    connect(allButton, &QPushButton::clicked, &dialog, [this, &findEdit, &replaceEdit] {
        if (findEdit.text().isEmpty()) { QMessageBox::warning(this, "替换", "查找内容不能为空。"); return; }
        editor->moveCursor(QTextCursor::Start);
        int count = 0;
        while (editor->find(findEdit.text())) { editor->textCursor().insertText(replaceEdit.text()); ++count; }
        QMessageBox::information(this, "替换完成", QString("共替换 %1 处。").arg(count));
    });
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void MainWindow::setFontForSelection()
{
    bool ok = false;
    const QFont font = QFontDialog::getFont(&ok, editor->currentCharFormat().font(), this, "选择字体");
    if (ok) {
        QTextCharFormat format;
        format.setFont(font);
        editor->mergeCurrentCharFormat(format);
    }
}

void MainWindow::setTextColor()
{
    const QColor color = QColorDialog::getColor(editor->currentCharFormat().foreground().color(), this, "选择文字颜色");
    if (color.isValid()) {
        QTextCharFormat format;
        format.setForeground(color);
        editor->mergeCurrentCharFormat(format);
    }
}

void MainWindow::setBackgroundColor()
{
    const QColor color = QColorDialog::getColor(editor->palette().color(QPalette::Base), this, "选择背景颜色");
    if (color.isValid()) {
        QPalette palette = editor->palette();
        palette.setColor(QPalette::Base, color);
        editor->setPalette(palette);
    }
}

void MainWindow::toggleWrap(bool checked)
{
    editor->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

void MainWindow::toggleAutoSave(bool checked)
{
    if (checked) autoSaveTimer->start(30000);
    else autoSaveTimer->stop();
    statusBar()->showMessage(checked ? "已开启每 30 秒自动保存" : "已关闭自动保存", 2500);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) event->accept();
    else event->ignore();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}



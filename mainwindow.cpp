#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "xlsxdocument.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtMath>

namespace {
const int LABEL_DPI = 203;
const double LABEL_WIDTH_MM = 50.0;
const double LABEL_HEIGHT_MM = 25.0;

int labelPixelWidth() {
    return qRound(LABEL_WIDTH_MM / 25.4 * LABEL_DPI);
}

int labelPixelHeight() {
    return qRound(LABEL_HEIGHT_MM / 25.4 * LABEL_DPI);
}

int digitRunLength(const QString &s, int index) {
    int count = 0;
    while (index + count < s.length() && s.at(index + count).isDigit()) {
        ++count;
    }
    return count;
}

int defaultFontForTemplate(int index) {
    return (index == 0 || index == 3) ? 36 : 8;
}

QString variantToExcelText(const QVariant &value) {
    if (!value.isValid() || value.isNull()) {
        return QString();
    }

    if (value.type() == QVariant::String) {
        return value.toString().trimmed();
    }

    if (value.type() == QVariant::Int ||
        value.type() == QVariant::LongLong ||
        value.type() == QVariant::UInt ||
        value.type() == QVariant::ULongLong) {
        return value.toString().trimmed();
    }

    if (value.type() == QVariant::Double) {
        const double number = value.toDouble();

        if (qAbs(number - qRound64(number)) < 0.0000001) {
            return QString::number(qRound64(number));
        }

        QString text = QString::number(number, 'f', 15);
        while (text.contains('.') && text.endsWith('0')) {
            text.chop(1);
        }
        if (text.endsWith('.')) {
            text.chop(1);
        }
        return text.trimmed();
    }

    return value.toString().trimmed();
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , printerComboBox(nullptr)
    , refreshButton(nullptr)
    , settingsButton(nullptr)
    , selectButton(nullptr)
    , printButton(nullptr)
    , fileLabel(nullptr)
    , countLabel(nullptr)
    , previewWidget(nullptr)
    , shelfFontSize(36)
    , accessoryFontSize(8)
    , assetFontSize(8)
    , textFontSize(36)
    , printCopies(1)
    , labelSizeIndex(0)
    , selectedTemplate(ShelfTemplate)
{
    ui->setupUi(this);
    loadSettings();
    buildUi();
    refreshPrinters();
    updateButtonState();
    updatePreview(); // 之前报错是因为下面没写这个函数的实现
}

MainWindow::~MainWindow() {
    saveSettings();
    delete ui;
}

// 核心修复 1：添加缺失的 updatePreview 实现
void MainWindow::updatePreview() {
    if (previewWidget) {
        previewWidget->update();
    }
}

void MainWindow::loadSettings() {
    QSettings settings("TSC_Label_Printer", "LabelPrinter");

    // 字体不再长期保存：每次打开软件按模板使用默认字号。
    // 货架默认 36，配件/资产默认 12。
    shelfFontSize = 36;
    accessoryFontSize = 8;
    assetFontSize = 8;
    textFontSize = 36;

    printCopies = settings.value("printCopies", 1).toInt();

    labelSizeIndex = settings.value("labelSizeIndex", 0).toInt();
    if (labelSizeIndex < 0 || labelSizeIndex > 5) {
        labelSizeIndex = 0;
    }

    int templateIndex = settings.value("template", 0).toInt();
    if (templateIndex == 1) selectedTemplate = AccessoryTemplate;
    else if (templateIndex == 2) selectedTemplate = AssetTemplate;
    else if (templateIndex == 3) selectedTemplate = TextTemplate;
    else selectedTemplate = ShelfTemplate;
}

void MainWindow::saveSettings() {
    QSettings settings("TSC_Label_Printer", "LabelPrinter");
    settings.setValue("printCopies", printCopies);
    settings.setValue("labelSizeIndex", labelSizeIndex);
    int index = 0;
    if (selectedTemplate == AccessoryTemplate) index = 1;
    else if (selectedTemplate == AssetTemplate) index = 2;
    else if (selectedTemplate == TextTemplate) index = 3;
    settings.setValue("template", index);
}

void MainWindow::buildUi() {
    setWindowTitle("TSC 标签打印软件 - 50mm x 25mm");
    setWindowIcon(QIcon(":/icons/app.ico"));
    qApp->setWindowIcon(QIcon(":/icons/app.ico"));

    QFont appFont("Microsoft YaHei", 14);
    qApp->setFont(appFont);

    setStyleSheet(
        "QMainWindow { background-color: #edf3f8; }"
        "QWidget#HeaderBar {"
        "  background: #ffffff;"
        "  border: 1px solid #d5e0ec;"
        "  border-radius: 12px;"
        "}"
        "QFrame#InfoPanel, QFrame#PreviewPanel {"
        "  background: white;"
        "  border: 1px solid #d5e0ec;"
        "  border-radius: 12px;"
        "}"
        "QLabel#PrinterLabel { color: #1e293b; font-size: 20px; font-weight: 700; }"
        "QLabel#FileLabel, QLabel#CountLabel { color: #334155; font-size: 18px; font-weight: 500; }"
        "QLabel#PreviewTitle { color: #0f172a; font-size: 20px; font-weight: 800; }"
        "QComboBox {"
        "  background: white;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 9px;"
        "  font-size: 20px;"
        "  min-height: 42px;"
        "  padding: 4px 8px;"
        "}"
        "QSpinBox {"
        "  background: white;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 9px;"
        "  font-size: 20px;"
        "  min-height: 42px;"
        "  padding: 4px 8px;"
        "}"
        "QPushButton {"
        "  color: white;"
        "  background-color: #3f7cf7;"
        "  border: none;"
        "  border-radius: 10px;"
        "  font-size: 18px;"
        "  font-weight: 700;"
        "  padding: 9px 18px;"
        "  min-height: 38px;"
        "}"
        "QPushButton:hover { background-color: #2f6ef0; }"
        "QPushButton:pressed { background-color: #235ad0; }"
        "QPushButton:disabled { background-color: #b7c4d8; color: #eef3f8; }"
        "QPushButton#settingsButton { background-color: #64748b; }"
        "QPushButton#settingsButton:hover { background-color: #556478; }"
        "QPushButton#refreshButton { background-color: #0ea5a4; }"
        "QPushButton#refreshButton:hover { background-color: #0b8d8d; }"
        "QPushButton#selectButton { background-color: #2563eb; }"
        "QPushButton#selectButton:hover { background-color: #1f56ce; }"
        "QPushButton#selectButton[selected='true'] { background-color: #16a34a; }"
        "QPushButton#printButton { background-color: #f97316; }"
        "QPushButton#printButton:hover { background-color: #e1640d; }"
        "QDialog { background-color: #f8fbff; }"
    );

    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(10);

    QWidget *headerBar = new QWidget(this);
    headerBar->setObjectName("HeaderBar");
    headerBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerBar->setFixedHeight(84);

    QHBoxLayout *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);

    QLabel *printerLabel = new QLabel("打印机：", this);
    printerLabel->setObjectName("PrinterLabel");

    printerComboBox = new QComboBox(this);
    printerComboBox->setMinimumWidth(220);
    printerComboBox->setMaximumWidth(280);
    printerComboBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    refreshButton = new QPushButton("刷新", this);
    settingsButton = new QPushButton("设置", this);
    selectButton = new QPushButton("选择 Excel", this);
    printButton = new QPushButton("打印标签", this);

    refreshButton->setObjectName("refreshButton");
    settingsButton->setObjectName("settingsButton");
    selectButton->setObjectName("selectButton");
    printButton->setObjectName("printButton");

    refreshButton->setMinimumSize(110, 50);
    settingsButton->setMinimumSize(110, 50);
    selectButton->setMinimumSize(165, 50);
    printButton->setMinimumSize(150, 50);

    refreshButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    settingsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    selectButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    printButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    headerLayout->addWidget(printerLabel);
    headerLayout->addWidget(printerComboBox);
    headerLayout->addWidget(refreshButton);
    headerLayout->addWidget(settingsButton);
    headerLayout->addWidget(selectButton);
    headerLayout->addStretch(1);
    headerLayout->addWidget(printButton);

    QFrame *infoPanel = new QFrame(this);
    infoPanel->setObjectName("InfoPanel");
    QVBoxLayout *infoLayout = new QVBoxLayout(infoPanel);
    infoLayout->setContentsMargins(16, 10, 16, 10);
    infoLayout->setSpacing(6);

    fileLabel = new QLabel("未选择 Excel 文件", this);
    fileLabel->setObjectName("FileLabel");
    fileLabel->setWordWrap(true);

    countLabel = new QLabel("标签数量：0", this);
    countLabel->setObjectName("CountLabel");

    infoLayout->addWidget(fileLabel);
    infoLayout->addWidget(countLabel);

    QFrame *previewPanel = new QFrame(this);
    previewPanel->setObjectName("PreviewPanel");
    QVBoxLayout *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(16, 12, 16, 14);
    previewLayout->setSpacing(8);

    QLabel *previewTitle = new QLabel("标签预览（自动显示第一张）", this);
    previewTitle->setObjectName("PreviewTitle");

    previewWidget = new PreviewWidget(this, this);
    previewWidget->setMinimumHeight(280);
    previewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    previewWidget->setStyleSheet("background-color: #f8fafc; border: 1px dashed #cbd5e1; border-radius: 12px;");

    previewLayout->addWidget(previewTitle);
    previewLayout->addWidget(previewWidget, 1);

    mainLayout->addWidget(headerBar);
    mainLayout->addWidget(infoPanel);
    mainLayout->addWidget(previewPanel, 1);
    setCentralWidget(central);

    connect(selectButton, &QPushButton::clicked, this, &MainWindow::selectExcelFile);
    connect(printButton, &QPushButton::clicked, this, &MainWindow::printLabels);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPrinters);
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);

    QScreen *screen = QApplication::primaryScreen();
    int windowWidth = 1220;
    int windowHeight = 760;

    if (screen) {
        const QRect available = screen->availableGeometry();

        // 使用固定逻辑尺寸，避免高分辨率下控件显得过小、低分辨率下控件被撑得过大。
        // 屏幕较小时自动收缩到可用区域内。
        windowWidth = qMin(windowWidth, qMax(980, available.width() - 80));
        windowHeight = qMin(windowHeight, qMax(640, available.height() - 80));
    }

    resize(windowWidth, windowHeight);
    setFixedSize(windowWidth, windowHeight);

    if (statusBar()) {
        statusBar()->hide();
    }
}

void MainWindow::refreshPrinters() {
    printerComboBox->clear();
    const QStringList blocked = {"onenote", "print to pdf", "microsoft print to pdf"};
    for (const QPrinterInfo &info : QPrinterInfo::availablePrinters()) {
        QString name = info.printerName().toLower();
        bool skip = false;
        for (const QString &b : blocked) {
            if (name.contains(b)) { skip = true; break; }
        }
        if (!skip) printerComboBox->addItem(info.printerName());
    }
    const QPrinterInfo def = QPrinterInfo::defaultPrinter();
    if (!def.isNull()) printerComboBox->setCurrentText(def.printerName());
}

void MainWindow::openSettingsDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("打印设置");
    dialog.setMinimumWidth(500);
    dialog.setStyleSheet(
        "QDialog { background-color: #f8fbff; }"
        "QLabel { font-size: 18px; color: #1e293b; }"
        "QComboBox, QSpinBox {"
        "  background: white;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 10px;"
        "  font-size: 20px;"
        "  min-height: 46px;"
        "  padding: 6px 10px;"
        "}"
        "QPushButton {"
        "  font-size: 20px;"
        "  min-height: 46px;"
        "  padding: 8px 18px;"
        "}"
    );

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(16);

    QFormLayout *form = new QFormLayout;
    form->setSpacing(14);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QComboBox *tempC = new QComboBox(&dialog);
    tempC->addItems({"货架", "配件", "资产", "文本"});
    tempC->setCurrentIndex(selectedTemplate == AccessoryTemplate ? 1 : (selectedTemplate == AssetTemplate ? 2 : (selectedTemplate == TextTemplate ? 3 : 0)));

    QSpinBox *fS = new QSpinBox(&dialog);
    fS->setRange(5, 60);
    fS->setValue(defaultFontForTemplate(tempC->currentIndex()));

    QSpinBox *cS = new QSpinBox(&dialog);
    cS->setRange(1, 999);
    cS->setValue(printCopies);

    QComboBox *sizeC = new QComboBox(&dialog);
    sizeC->addItems({"50×25 默认", "60×30", "70×30", "70×40", "80×40", "100×50"});
    sizeC->setCurrentIndex(labelSizeIndex);

    connect(tempC, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [fS](int index) {
        fS->setValue(defaultFontForTemplate(index));
    });

    form->addRow("标签模板：", tempC);
    form->addRow("字体大小：", fS);
    form->addRow("打印份数：", cS);
    form->addRow("输出尺寸：", sizeC);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, &dialog);
    form->addRow(bb);

    layout->addLayout(form);

    connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        int idx = tempC->currentIndex();
        if (idx == 1) { selectedTemplate = AccessoryTemplate; accessoryFontSize = fS->value(); }
        else if (idx == 2) { selectedTemplate = AssetTemplate; assetFontSize = fS->value(); }
        else if (idx == 3) { selectedTemplate = TextTemplate; textFontSize = fS->value(); }
        else { selectedTemplate = ShelfTemplate; shelfFontSize = fS->value(); }

        printCopies = cS->value();
        labelSizeIndex = sizeC->currentIndex();
        saveSettings();
        if (!excelFilePath.isEmpty()) reloadExcelByCurrentTemplate();
        updateButtonState();
        updatePreview();
    }
}

MainWindow::TemplateType MainWindow::currentTemplate() const { return selectedTemplate; }
int MainWindow::currentFontSize() const {
    if (selectedTemplate == ShelfTemplate) return shelfFontSize;
    if (selectedTemplate == TextTemplate) return textFontSize;
    return selectedTemplate == AccessoryTemplate ? accessoryFontSize : assetFontSize;
}
int MainWindow::currentCopies() const { return printCopies; }

double MainWindow::currentLabelWidthMm() const {
    switch (labelSizeIndex) {
    case 1: return 60.0;
    case 2: return 70.0;
    case 3: return 70.0;
    case 4: return 80.0;
    case 5: return 100.0;
    default: return 50.0;
    }
}

double MainWindow::currentLabelHeightMm() const {
    switch (labelSizeIndex) {
    case 1: return 30.0;
    case 2: return 30.0;
    case 3: return 40.0;
    case 4: return 40.0;
    case 5: return 50.0;
    default: return 25.0;
    }
}

int MainWindow::currentLabelPixelWidth() const {
    return qRound(currentLabelWidthMm() / 25.4 * LABEL_DPI);
}

int MainWindow::currentLabelPixelHeight() const {
    return qRound(currentLabelHeightMm() / 25.4 * LABEL_DPI);
}

bool MainWindow::isVirtualPdfPrinter(QPrinter &printer) const {
    QString name = printer.printerName().toLower();
    return printer.outputFormat() == QPrinter::PdfFormat || name.contains("pdf") || name.contains("xps") || name.contains("virtual");
}

QRectF MainWindow::labelTargetRect(QPrinter &printer) const {
    const QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
    if (!pageRect.isValid() || pageRect.width() <= 0) {
        return QRectF(0, 0, labelPixelWidth(), labelPixelHeight());
    }

    if (isVirtualPdfPrinter(printer)) {
        // PDF / 虚拟打印机：按虚拟页面区域输出，主要用于调试。
        return QRectF(pageRect.left(), pageRect.top(), pageRect.width(), pageRect.height());
    }

    // 实体 TSC 标签机：
    // 不使用 pageRect 的宽高决定标签大小，只把 pageRect.left/top 当作绘制起点。
    // 标签内容固定按当前标签尺寸 @ 203DPI 输出，不做软件缩放，也不做内容旋转。
    return QRectF(pageRect.left(), pageRect.top(), currentLabelPixelWidth(), currentLabelPixelHeight());
}

void MainWindow::printLabels() {
    if (printerComboBox->currentText().isEmpty()) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setPrinterName(printerComboBox->currentText());

    const bool virtualPrinter = isVirtualPdfPrinter(printer);

    if (virtualPrinter) {
        // PDF / 虚拟打印机：用于调试时输出标签页。
        printer.setPageSize(QPagedPaintDevice::Custom);
        printer.setPaperSize(QSizeF(currentLabelWidthMm(), currentLabelHeightMm()), QPrinter::Millimeter);
        printer.setOrientation(QPrinter::Portrait);
    } else {
        // 实体 TSC：只声明标签尺寸，不主动设置 orientation。
        // 方向交给打印机驱动/当前打印机设置处理，避免软件覆盖已经调好的方向。
        printer.setPageSize(QPagedPaintDevice::Custom);
        printer.setPaperSize(QSizeF(currentLabelWidthMm(), currentLabelHeightMm()), QPrinter::Millimeter);

        // 固定标签机点阵基准，避免显示器分辨率影响标签画布。
        printer.setResolution(LABEL_DPI);
    }

    printer.setPageMargins(0, 0, 0, 0, QPrinter::Millimeter);
    printer.setFullPage(true);

    bool ok = false;
    if (currentTemplate() == ShelfTemplate) ok = printShelfLabels(printer);
    else if (currentTemplate() == AccessoryTemplate) ok = printAccessoryLabels(printer);
    else if (currentTemplate() == AssetTemplate) ok = printAssetLabels(printer);
    else ok = printTextLabels(printer);

    if (ok) QMessageBox::information(this, "成功", "打印任务已发送");
    else QMessageBox::critical(this, "失败", "无法启动打印");
}

bool MainWindow::printShelfLabels(QPrinter &printer) {
    QPainter painter;
    if (!painter.begin(&printer)) return false;

    QRectF pageRect = labelTargetRect(printer);
    const bool virtualPrinter = isVirtualPdfPrinter(printer);

    for (int copy = 0; copy < currentCopies(); ++copy) {
        for (int i = 0; i < shelfLabels.count(); ++i) {
            if (copy > 0 || i > 0) printer.newPage();
            ShelfLabelData d;
            d.code = shelfLabels.at(i);
            if (virtualPrinter) {
                painter.drawImage(pageRect, createShelfLabelImage(d));
            } else {
                drawShelfLabel(painter, pageRect, d);
            }
        }
    }

    painter.end();
    return true;
}

bool MainWindow::printAccessoryLabels(QPrinter &printer) {
    QPainter painter;
    if (!painter.begin(&printer)) return false;

    QRectF pageRect = labelTargetRect(printer);
    const bool virtualPrinter = isVirtualPdfPrinter(printer);

    for (int copy = 0; copy < currentCopies(); ++copy) {
        for (int i = 0; i < accessoryLabels.count(); ++i) {
            if (copy > 0 || i > 0) printer.newPage();
            const AccessoryLabelData &d = accessoryLabels.at(i);
            if (virtualPrinter) {
                painter.drawImage(pageRect, createAccessoryLabelImage(d));
            } else {
                drawAccessoryLabel(painter, pageRect, d);
            }
        }
    }

    painter.end();
    return true;
}

bool MainWindow::printAssetLabels(QPrinter &printer) {
    QPainter painter;
    if (!painter.begin(&printer)) return false;

    QRectF pageRect = labelTargetRect(printer);
    const bool virtualPrinter = isVirtualPdfPrinter(printer);

    for (int copy = 0; copy < currentCopies(); ++copy) {
        for (int i = 0; i < assetLabels.count(); ++i) {
            if (copy > 0 || i > 0) printer.newPage();
            const AssetLabelData &d = assetLabels.at(i);
            if (virtualPrinter) {
                painter.drawImage(pageRect, createAssetLabelImage(d));
            } else {
                drawAssetLabel(painter, pageRect, d);
            }
        }
    }

    painter.end();
    return true;
}

bool MainWindow::printTextLabels(QPrinter &printer) {
    QPainter painter;
    if (!painter.begin(&printer)) return false;

    QRectF pageRect = labelTargetRect(printer);
    const bool virtualPrinter = isVirtualPdfPrinter(printer);

    for (int copy = 0; copy < currentCopies(); ++copy) {
        for (int i = 0; i < textLabels.count(); ++i) {
            if (copy > 0 || i > 0) printer.newPage();
            TextLabelData d;
            d.text = textLabels.at(i);
            if (virtualPrinter) {
                painter.drawImage(pageRect, createTextLabelImage(d));
            } else {
                drawTextLabel(painter, pageRect, d);
            }
        }
    }

    painter.end();
    return true;
}

QImage MainWindow::createShelfLabelImage(const ShelfLabelData &data) {
    QImage img(currentLabelPixelWidth(), currentLabelPixelHeight(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    drawShelfLabel(p, QRectF(0,0,img.width(),img.height()), data);
    return img;
}

QImage MainWindow::createTextLabelImage(const TextLabelData &data) {
    QImage img(currentLabelPixelWidth(), currentLabelPixelHeight(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    drawTextLabel(p, QRectF(0,0,img.width(),img.height()), data);
    return img;
}

QImage MainWindow::createAccessoryLabelImage(const AccessoryLabelData &data) {
    QImage img(currentLabelPixelWidth(), currentLabelPixelHeight(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    drawAccessoryLabel(p, QRectF(0,0,img.width(),img.height()), data);
    return img;
}

QImage MainWindow::createAssetLabelImage(const AssetLabelData &data) {
    QImage img(currentLabelPixelWidth(), currentLabelPixelHeight(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    drawAssetLabel(p, QRectF(0,0,img.width(),img.height()), data);
    return img;
}

void MainWindow::drawShelfLabel(QPainter &painter, const QRectF &rect, const ShelfLabelData &data) {
    painter.save();
    QRectF content = rect.adjusted(rect.width()*0.05, rect.height()*0.05, -rect.width()*0.05, -rect.height()*0.05);
    QFont f("Arial"); f.setBold(true); f.setPixelSize(pointToPrinterPixel(shelfFontSize));
    f = fittedFont(data.code, f, content.adjusted(0,0,0,-content.height()*0.3), 18);
    painter.setFont(f);
    painter.drawText(content.adjusted(0,0,0,-content.height()*0.3), Qt::AlignCenter, data.code);
    drawCode128(painter, content.adjusted(content.width()*0.1, content.height()*0.7, -content.width()*0.1, 0), data.code);
    painter.restore();
}

void MainWindow::drawAccessoryLabel(QPainter &painter, const QRectF &rect, const AccessoryLabelData &data) {
    painter.save();

    const QString company = QStringLiteral("上海卫星工程研究所");
    QRectF c = rect.adjusted(rect.width()*0.025, rect.height()*0.025,
                             -rect.width()*0.025, -rect.height()*0.025);

    QRectF companyRect(c.left(), c.top(), c.width(), c.height()*0.18);
    QFont titleF("Microsoft YaHei");
    titleF.setBold(true);
    titleF.setPixelSize(pointToPrinterPixel(12));
    titleF = fittedFont(company, titleF, companyRect, 10);

    painter.setFont(titleF);
    painter.drawText(companyRect, Qt::AlignCenter, company);

    double lineY = c.top() + c.height()*0.205;
    painter.drawLine(QPointF(c.left(), lineY), QPointF(c.right(), lineY));

    double y = c.top() + c.height()*0.235;
    double rowH = c.height()*0.145;

    QRectF nameRect(c.left(), y, c.width(), rowH);
    QRectF accessoryRect(c.left(), y + rowH, c.width(), rowH);
    QRectF noRect(c.left(), y + rowH * 2.0, c.width(), rowH);

    QString nameText = "资产名称：" + data.assetName;
    QString accessoryText = "配件名称：" + data.accessoryName;
    QString noText = "资产编号：" + data.assetNo;

    QFont editableF("Microsoft YaHei");
    editableF.setPixelSize(pointToPrinterPixel(accessoryFontSize));

    QFont nameF = fittedFont(nameText, editableF, nameRect, 7);
    QFont accessoryF = fittedFont(accessoryText, editableF, accessoryRect, 7);

    // 编号通常最长，单独允许缩到更小，避免出界。
    QFont noF("Microsoft YaHei");
    noF.setPixelSize(pointToPrinterPixel(accessoryFontSize));
    noF = fittedFont(noText, noF, noRect, 5);

    painter.setFont(nameF);
    painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, nameText);

    painter.setFont(accessoryF);
    painter.drawText(accessoryRect, Qt::AlignLeft | Qt::AlignVCenter, accessoryText);

    painter.setFont(noF);
    painter.drawText(noRect, Qt::AlignLeft | Qt::AlignVCenter, noText);

    QRectF barcodeRect(c.left()+c.width()*0.03,
                       c.top()+c.height()*0.725,
                       c.width()*0.94,
                       c.height()*0.245);
    drawCode128(painter, barcodeRect, data.assetNo);

    painter.restore();
}

void MainWindow::drawAssetLabel(QPainter &painter, const QRectF &rect, const AssetLabelData &data) {
    painter.save();

    const QString company = QStringLiteral("上海卫星工程研究所");
    QRectF c = rect.adjusted(rect.width()*0.030, rect.height()*0.030,
                             -rect.width()*0.030, -rect.height()*0.030);

    QRectF companyRect(c.left(), c.top(), c.width(), c.height()*0.20);
    QFont tf("Microsoft YaHei");
    tf.setBold(true);
    tf.setPixelSize(pointToPrinterPixel(12));
    tf = fittedFont(company, tf, companyRect, 10);

    painter.setFont(tf);
    painter.drawText(companyRect, Qt::AlignCenter, company);

    double lineY = c.top()+c.height()*0.225;
    painter.drawLine(QPointF(c.left(), lineY), QPointF(c.right(), lineY));

    double y = c.top()+c.height()*0.285;
    double rowH = c.height()*0.17;

    QRectF nameRect(c.left(), y, c.width(), rowH);
    QRectF noRect(c.left(), y + rowH, c.width(), rowH);

    QString nameText = "资产名称：" + data.assetName;
    QString noText = "资产编号：" + data.assetNo;

    QFont infoF("Microsoft YaHei");
    infoF.setPixelSize(pointToPrinterPixel(assetFontSize));

    QFont nameF = fittedFont(nameText, infoF, nameRect, 7);

    // 编号通常最长，单独允许缩到更小，避免出界。
    QFont noF("Microsoft YaHei");
    noF.setPixelSize(pointToPrinterPixel(assetFontSize));
    noF = fittedFont(noText, noF, noRect, 5);

    painter.setFont(nameF);
    painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, nameText);

    painter.setFont(noF);
    painter.drawText(noRect, Qt::AlignLeft | Qt::AlignVCenter, noText);

    QRectF barcodeRect(c.left()+c.width()*0.03,
                       c.top()+c.height()*0.700,
                       c.width()*0.94,
                       c.height()*0.265);
    drawCode128(painter, barcodeRect, data.assetNo);

    painter.restore();
}

void MainWindow::drawTextLabel(QPainter &painter, const QRectF &rect, const TextLabelData &data) {
    painter.save();
    QRectF content = rect.adjusted(rect.width()*0.05, rect.height()*0.05,
                                   -rect.width()*0.05, -rect.height()*0.05);
    QFont f("Microsoft YaHei");
    f.setBold(false);
    f.setPixelSize(pointToPrinterPixel(textFontSize));
    f = fittedFont(data.text, f, content, 18);
    painter.setFont(f);
    painter.drawText(content, Qt::AlignCenter | Qt::TextWordWrap, data.text);
    painter.restore();
}

void MainWindow::selectExcelFile() {
    QString path = QFileDialog::getOpenFileName(this, "选择 Excel", "", "Excel (*.xlsx)");
    if (path.isEmpty()) return;
    excelFilePath = path;
    reloadExcelByCurrentTemplate();

    selectButton->setProperty("selected", true);
    selectButton->style()->unpolish(selectButton);
    selectButton->style()->polish(selectButton);
    selectButton->update();

    updateButtonState();
    updatePreview();
}

void MainWindow::reloadExcelByCurrentTemplate() {
    if (excelFilePath.isEmpty()) return;
    if (selectedTemplate == ShelfTemplate) shelfLabels = readShelfLabelsFromExcel(excelFilePath);
    else if (selectedTemplate == AccessoryTemplate) accessoryLabels = readAccessoryLabelsFromExcel(excelFilePath);
    else if (selectedTemplate == AssetTemplate) assetLabels = readAssetLabelsFromExcel(excelFilePath);
    else textLabels = readTextLabelsFromExcel(excelFilePath);

    fileLabel->setText("已载入: " + excelFilePath);
    int count = 0;
    if (selectedTemplate == ShelfTemplate) count = shelfLabels.size();
    else if (selectedTemplate == AccessoryTemplate) count = accessoryLabels.size();
    else if (selectedTemplate == AssetTemplate) count = assetLabels.size();
    else count = textLabels.size();
    countLabel->setText(QString("标签数量: %1").arg(count));
}

// 核心修复 2：修正所有警告 (-Wmisleading-indentation)
// 将 if 语句和后续操作分行显示，或增加大括号
QStringList MainWindow::readShelfLabelsFromExcel(const QString &filePath) {
    QStringList res; QXlsx::Document xlsx(filePath);
    for (int r = 2; ; ++r) {
        QString s = variantToExcelText(xlsx.read(r, 1));
        if (s.isEmpty()) {
            break;
        }
        res << s;
    }
    return res;
}

QVector<AccessoryLabelData> MainWindow::readAccessoryLabelsFromExcel(const QString &filePath) {
    QVector<AccessoryLabelData> res; QXlsx::Document xlsx(filePath);
    for (int r = 2; ; ++r) {
        AccessoryLabelData d;
        d.assetName = xlsx.read(r, 1).toString().trimmed();
        d.accessoryName = xlsx.read(r, 2).toString().trimmed();
        d.assetNo = variantToExcelText(xlsx.read(r, 3));
        if (d.assetName.isEmpty()) {
            break;
        }
        res << d;
    }
    return res;
}

QVector<AssetLabelData> MainWindow::readAssetLabelsFromExcel(const QString &filePath) {
    QVector<AssetLabelData> res; QXlsx::Document xlsx(filePath);
    for (int r = 2; ; ++r) {
        AssetLabelData d;
        d.assetName = xlsx.read(r, 1).toString().trimmed();
        d.assetNo = variantToExcelText(xlsx.read(r, 2));
        if (d.assetName.isEmpty()) {
            break;
        }
        res << d;
    }
    return res;
}

QStringList MainWindow::readTextLabelsFromExcel(const QString &filePath) {
    QStringList res; QXlsx::Document xlsx(filePath);
    for (int r = 2; ; ++r) {
        QString s = variantToExcelText(xlsx.read(r, 1));
        if (s.isEmpty()) {
            break;
        }
        res << s;
    }
    return res;
}

void MainWindow::updateButtonState() {
    bool has = false;
    if (selectedTemplate == ShelfTemplate) has = !shelfLabels.isEmpty();
    else if (selectedTemplate == AccessoryTemplate) has = !accessoryLabels.isEmpty();
    else if (selectedTemplate == AssetTemplate) has = !assetLabels.isEmpty();
    else has = !textLabels.isEmpty();
    printButton->setEnabled(has);
}

void MainWindow::renderPreview(QPainter &p, const QRectF &rect) {
    p.fillRect(rect, QColor("#f0f0f0"));
    double ratio = currentLabelWidthMm() / currentLabelHeightMm();
    double w = rect.width() * 0.8; double h = w / ratio;
    if (h > rect.height()*0.8) { h = rect.height()*0.8; w = h * ratio; }
    QRectF r(rect.center().x() - w/2, rect.center().y() - h/2, w, h);
    p.setBrush(Qt::white); p.setPen(QPen(Qt::black, 1));
    p.drawRect(r);
    
    bool has = false;
    if (selectedTemplate == ShelfTemplate) has = !shelfLabels.isEmpty();
    else if (selectedTemplate == AccessoryTemplate) has = !accessoryLabels.isEmpty();
    else if (selectedTemplate == AssetTemplate) has = !assetLabels.isEmpty();
    else has = !textLabels.isEmpty();

    if (has) {
        QImage img;
        if (selectedTemplate == ShelfTemplate) {
            ShelfLabelData d; d.code = shelfLabels.first(); img = createShelfLabelImage(d);
        } else if (selectedTemplate == AccessoryTemplate) {
            img = createAccessoryLabelImage(accessoryLabels.first());
        } else if (selectedTemplate == AssetTemplate) {
            img = createAssetLabelImage(assetLabels.first());
        } else {
            TextLabelData d; d.text = textLabels.first(); img = createTextLabelImage(d);
        }
        p.drawImage(r, img);
    }
}

int MainWindow::pointToPrinterPixel(int pt) const { return qMax(1, qRound(pt * LABEL_DPI / 72.0)); }

QFont MainWindow::fittedFont(const QString &text, QFont font, const QRectF &rect, int minSize) const {
    int px = font.pixelSize() > 0 ? font.pixelSize() : pointToPrinterPixel(font.pointSize());
    font.setPixelSize(px);
    while (px > minSize) {
        QFontMetrics fm(font);
        if (fm.boundingRect(text).width() <= rect.width()*0.95 && fm.boundingRect(text).height() <= rect.height()) break;
        font.setPixelSize(--px);
    }
    return font;
}

QString MainWindow::code128Pattern(int v) const {
    static const char *p[] = { "212222","222122","222221","121223","121322","131222","122213","122312","132212","221213","221312","231212","112232","122132","122231","113222","123122","123221","223211","221132","221231","213212","223112","312131","311222","321122","321221","312212","322112","322211","212123","212321","232121","111323","131123","131321","112313","132113","132311","211313","231113","231311","112133","112331","132131","113123","113321","133121","313121","211331","231131","213113","213311","213131","311123","311321","331121","312113","312311","332111","314111","221411","431111","111224","111422","121124","121421","141122","141221","112214","112412","122114","122411","142112","142211","241211","221114","413111","241112","134111","111242","121142","121241","114212","124112","124211","411212","421112","421211","212141","214121","412121","111143","111341","131141","114113","114311","411113","411311","113141","114131","311141","411131","211412","211214","211232","2331112" };
    return (v>=0 && v<=106) ? QString::fromLatin1(p[v]) : "";
}

QVector<int> MainWindow::encodeCode128Auto(const QString &text) const {
    QVector<int> codes; QString data = text.trimmed(); if(data.isEmpty()) return codes;
    int i=0; bool c=false;
    if(digitRunLength(data,0)>=4) { codes.append(105); c=true; } else codes.append(104);
    while(i<data.length()){
        if(c){
            int r=digitRunLength(data,i);
            if(r>=2){ codes.append(data.mid(i,2).toInt()); i+=2; continue; }
            codes.append(100); c=false; continue;
        }
        if(digitRunLength(data,i)>=4){ codes.append(99); c=true; continue; }
        int u=data.at(i).unicode(); codes.append((u<32||u>127)?'?'-32:u-32); i++;
    }
    int sum=codes[0]; for(int p=1;p<codes.size();++p) sum+=codes[p]*p;
    codes.append(sum%103); codes.append(106); return codes;
}

void MainWindow::drawCode128(QPainter &p, const QRectF &r, const QString &t) {
    QVector<int> codes = encodeCode128Auto(t); if(codes.isEmpty()) return;
    int mods=0; for(int v:codes) { for(QChar ch:code128Pattern(v)) mods+=ch.digitValue(); }
    double mw = r.width()/(mods+16); double x = r.left()+8*mw;
    p.save(); p.setRenderHint(QPainter::Antialiasing, false); p.setBrush(Qt::black); p.setPen(Qt::NoPen);
    for(int v:codes){
        QString pat = code128Pattern(v);
        for(int i=0;i<pat.length();++i){
            double w = pat.at(i).digitValue()*mw;
            if(i%2==0) p.drawRect(QRectF(x, r.top(), w, r.height()));
            x+=w;
        }
    }
    p.restore();
}

PreviewWidget::PreviewWidget(MainWindow *w, QWidget *p) : QWidget(p), mainWindow(w) {}
void PreviewWidget::paintEvent(QPaintEvent *) { QPainter p(this); p.setRenderHint(QPainter::Antialiasing); if(mainWindow) mainWindow->renderPreview(p, rect()); }
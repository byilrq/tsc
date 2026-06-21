#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QVector>
#include <QImage>
#include <QWidget>
#include <QRectF>

class QComboBox;
class QPushButton;
class QLabel;
class QPrinter;
class QPainter;
class QPaintEvent;
class PreviewWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct ShelfLabelData
{
    QString code;
};

struct AccessoryLabelData
{
    QString assetName;
    QString accessoryName;
    QString assetNo;
};

struct AssetLabelData
{
    QString assetName;
    QString assetNo;
};

struct TextLabelData
{
    QString text;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void renderPreview(QPainter &painter, const QRectF &rect);

private slots:
    void selectExcelFile();
    void printLabels();
    void refreshPrinters();
    void openSettingsDialog();
    void updatePreview();

private:
    enum TemplateType {
        ShelfTemplate,
        AccessoryTemplate,
        AssetTemplate,
        TextTemplate
    };

    void buildUi();
    void loadSettings();
    void saveSettings();
    void updateButtonState();
    void reloadExcelByCurrentTemplate();

    QStringList readShelfLabelsFromExcel(const QString &filePath);
    QVector<AccessoryLabelData> readAccessoryLabelsFromExcel(const QString &filePath);
    QVector<AssetLabelData> readAssetLabelsFromExcel(const QString &filePath);
    QStringList readTextLabelsFromExcel(const QString &filePath);

    bool printShelfLabels(QPrinter &printer);
    bool printAccessoryLabels(QPrinter &printer);
    bool printAssetLabels(QPrinter &printer);
    bool printTextLabels(QPrinter &printer);

    QImage createShelfLabelImage(const ShelfLabelData &data);
    QImage createAccessoryLabelImage(const AccessoryLabelData &data);
    QImage createAssetLabelImage(const AssetLabelData &data);
    QImage createTextLabelImage(const TextLabelData &data);

    void drawShelfLabel(QPainter &painter, const QRectF &pageRect, const ShelfLabelData &data);
    void drawAccessoryLabel(QPainter &painter, const QRectF &pageRect, const AccessoryLabelData &data);
    void drawAssetLabel(QPainter &painter, const QRectF &pageRect, const AssetLabelData &data);
    void drawTextLabel(QPainter &painter, const QRectF &pageRect, const TextLabelData &data);

    void drawCode128(QPainter &painter, const QRectF &rect, const QString &text);
    QVector<int> encodeCode128Auto(const QString &text) const;
    QString code128Pattern(int value) const;

    QFont fittedFont(const QString &text, QFont font, const QRectF &rect, int minPixelSize) const;
    int pointToPrinterPixel(int pointSize) const;

    TemplateType currentTemplate() const;
    int currentFontSize() const;
    int currentCopies() const;
    double currentLabelWidthMm() const;
    double currentLabelHeightMm() const;
    int currentLabelPixelWidth() const;
    int currentLabelPixelHeight() const;
    bool isVirtualPdfPrinter(QPrinter &printer) const;
    QRectF labelTargetRect(QPrinter &printer) const;

private:
    Ui::MainWindow *ui;

    QComboBox *printerComboBox;
    QPushButton *refreshButton;
    QPushButton *settingsButton;
    QPushButton *selectButton;
    QPushButton *printButton;

    QLabel *fileLabel;
    QLabel *countLabel;
    PreviewWidget *previewWidget;

    int shelfFontSize;
    int accessoryFontSize;
    int assetFontSize;
    int textFontSize;
    int printCopies;
    int labelSizeIndex;
    TemplateType selectedTemplate;

    QString excelFilePath;
    QStringList shelfLabels;
    QVector<AccessoryLabelData> accessoryLabels;
    QVector<AssetLabelData> assetLabels;
    QStringList textLabels;
};

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewWidget(MainWindow *window, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    MainWindow *mainWindow;
};

#endif // MAINWINDOW_H

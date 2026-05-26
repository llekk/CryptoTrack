#include "ai_project.h"

#include <algorithm>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QCoreApplication>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollBar>
#include <QSslSocket>
#include <QToolTip>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtMath>


class MiniChartWidget : public QWidget
{
public:
    enum ChartType
    {
        LineChart,
        CandleChart,
        BarChart,
        RsiChart
    };

    explicit MiniChartWidget(QWidget *parent = nullptr)
        : QWidget(parent),
        chartType(LineChart)
    {
        setMouseTracking(true);
        setMinimumHeight(150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setChartType(ChartType type)
    {
        chartType = type;
        update();
    }

    void setValues(const QVector<double> &newValues)
    {
        values = newValues;
        update();
    }

    void setCandles(const QVector<CandleUiData> &newCandles)
    {
        candles = newCandles;
        update();
    }

    void setSupportResistanceLevels(const QVector<double> &supports,
                                    const QVector<double> &resistances)
    {
        supportLevels = supports;
        resistanceLevels = resistances;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QRect area = chartArea();

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#121a2e"));
        painter.drawRoundedRect(rect(), 18, 18);

        int visibleCount = itemCount();

        if (visibleCount < 2)
        {
            painter.setPen(QColor("#8f9bb3"));
            painter.drawText(area, Qt::AlignCenter, "Недостаточно данных");
            return;
        }

        double minValue = 0.0;
        double maxValue = 0.0;

        findValueRange(minValue, maxValue);

        if (chartType == RsiChart)
        {
            minValue = 0.0;
            maxValue = 100.0;
        }

        if (qFuzzyCompare(minValue, maxValue))
        {
            maxValue += 1.0;
            minValue -= 1.0;
        }

        drawGrid(painter, area);
        drawAxisLabels(painter, area, minValue, maxValue);

        if (chartType == BarChart)
        {
            drawBars(painter, area, minValue, maxValue);
        }
        else if (chartType == CandleChart)
        {
            drawCandles(painter, area, minValue, maxValue);
            drawSupportResistance(painter, area, minValue, maxValue);
        }
        else if (chartType == RsiChart)
        {
            drawRsi(painter, area);
        }
        else
        {
            drawLine(painter, area, minValue, maxValue);
            drawSupportResistance(painter, area, minValue, maxValue);
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        QRect area = chartArea();

        if (!area.contains(event->pos()) || itemCount() < 2)
        {
            QToolTip::hideText();
            return;
        }

        int index = indexByX(event->pos().x(), area, itemCount());

        if (index < 0 || index >= itemCount())
        {
            QToolTip::hideText();
            return;
        }

        QString text;

        if (chartType == CandleChart && !candles.isEmpty())
        {
            const CandleUiData &candle = candles[index];

            if (qFuzzyIsNull(candle.open))
                return;

            double percent = (candle.close - candle.open) / candle.open * 100.0;

            QString direction;

            if (percent > 0.0)
                direction = "рост";
            else if (percent < 0.0)
                direction = "падение";
            else
                direction = "без изменения";

            text =
                "Свеча №" + QString::number(index + 1) +
                "\nOpen: " + formatChartValue(candle.open) +
                "\nHigh: " + formatChartValue(candle.high) +
                "\nLow: " + formatChartValue(candle.low) +
                "\nClose: " + formatChartValue(candle.close) +
                "\nИзменение: " + QString(percent >= 0 ? "+" : "") +
                QString::number(percent, 'f', 2) + "% (" + direction + ")";
        }
        else if (chartType == BarChart)
        {
            text =
                "Столбец №" + QString::number(index + 1) +
                "\nОбъём: " + formatChartValue(values[index]);
        }
        else if (chartType == RsiChart)
        {
            text =
                "Точка №" + QString::number(index + 1) +
                "\nRSI: " + QString::number(values[index], 'f', 2);
        }
        else
        {
            text =
                "Точка №" + QString::number(index + 1) +
                "\nЦена: " + formatChartValue(values[index]);
        }

        QToolTip::showText(event->globalPosition().toPoint(), text, this);
    }

    void leaveEvent(QEvent *) override
    {
        QToolTip::hideText();
    }

private:
    QVector<double> values;
    QVector<CandleUiData> candles;
    QVector<double> supportLevels;
    QVector<double> resistanceLevels;
    ChartType chartType;

    QRect chartArea() const
    {
        return rect().adjusted(62, 18, -18, -36);
    }

    int itemCount() const
    {
        if (chartType == CandleChart && !candles.isEmpty())
            return candles.size();

        return values.size();
    }

    void findValueRange(double &minValue, double &maxValue) const
    {
        if (chartType == CandleChart && !candles.isEmpty())
        {
            minValue = candles[0].low;
            maxValue = candles[0].high;

            for (const CandleUiData &candle : candles)
            {
                minValue = qMin(minValue, candle.low);
                maxValue = qMax(maxValue, candle.high);
            }

            return;
        }

        minValue = values[0];
        maxValue = values[0];

        for (double value : values)
        {
            minValue = qMin(minValue, value);
            maxValue = qMax(maxValue, value);
        }
    }

    int indexByX(int x, const QRect &area, int count) const
    {
        if (count <= 0)
            return -1;

        double position = static_cast<double>(x - area.left()) / area.width();
        int index = qRound(position * (count - 1));

        return qBound(0, index, count - 1);
    }

    QString formatChartValue(double value) const
    {
        double absValue = qAbs(value);

        if (absValue >= 1000000000.0)
            return QString::number(value / 1000000000.0, 'f', 2) + "B";

        if (absValue >= 1000000.0)
            return QString::number(value / 1000000.0, 'f', 2) + "M";

        if (absValue >= 1000.0)
            return QString::number(value, 'f', 0);

        if (absValue >= 1.0)
            return QString::number(value, 'f', 2);

        return QString::number(value, 'f', 4);
    }

    void drawGrid(QPainter &painter, const QRect &area)
    {
        painter.setPen(QPen(QColor("#26314f"), 1));

        for (int i = 0; i <= 4; ++i)
        {
            int y = area.top() + area.height() * i / 4;
            painter.drawLine(area.left(), y, area.right(), y);
        }

        for (int i = 0; i <= 6; ++i)
        {
            int x = area.left() + area.width() * i / 6;
            painter.drawLine(x, area.top(), x, area.bottom());
        }

        painter.setPen(QPen(QColor("#3b4a6b"), 1));
        painter.drawLine(area.left(), area.top(), area.left(), area.bottom());
        painter.drawLine(area.left(), area.bottom(), area.right(), area.bottom());
    }

    void drawAxisLabels(QPainter &painter, const QRect &area, double minValue, double maxValue)
    {
        painter.setPen(QColor("#8f9bb3"));
        painter.setFont(QFont("Segoe UI", 8));

        for (int i = 0; i <= 4; ++i)
        {
            double value = maxValue - (maxValue - minValue) * i / 4.0;
            int y = area.top() + area.height() * i / 4;

            QRect labelRect(6, y - 9, area.left() - 10, 18);
            painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, formatChartValue(value));
        }

        int count = itemCount();

        if (count > 1)
        {
            QVector<int> marks;
            marks.append(1);
            marks.append(qMax(1, count / 2));
            marks.append(count);

            for (int mark : marks)
            {
                int index = mark - 1;
                double x = area.left() + static_cast<double>(area.width()) * index / (count - 1);

                QRect labelRect(static_cast<int>(x) - 24, area.bottom() + 8, 48, 18);
                painter.drawText(labelRect, Qt::AlignCenter, QString::number(mark));
            }
        }
    }

    double mapY(double value, const QRect &area, double minValue, double maxValue) const
    {
        double normalized = (value - minValue) / (maxValue - minValue);
        return area.bottom() - normalized * area.height();
    }

    QPointF pointAt(int index, const QRect &area, double minValue, double maxValue)
    {
        double x = area.left() + static_cast<double>(area.width()) * index / (values.size() - 1);
        double y = mapY(values[index], area, minValue, maxValue);

        return QPointF(x, y);
    }

    void drawLine(QPainter &painter, const QRect &area, double minValue, double maxValue)
    {
        QPainterPath path;
        path.moveTo(pointAt(0, area, minValue, maxValue));

        for (int i = 1; i < values.size(); ++i)
            path.lineTo(pointAt(i, area, minValue, maxValue));

        QLinearGradient gradient(area.topLeft(), area.bottomLeft());
        gradient.setColorAt(0.0, QColor(34, 197, 94, 90));
        gradient.setColorAt(1.0, QColor(34, 197, 94, 0));

        QPainterPath fillPath = path;
        fillPath.lineTo(area.right(), area.bottom());
        fillPath.lineTo(area.left(), area.bottom());
        fillPath.closeSubpath();

        painter.fillPath(fillPath, gradient);

        painter.setPen(QPen(QColor("#22c55e"), 3));
        painter.drawPath(path);

        painter.setBrush(QColor("#22c55e"));
        painter.setPen(Qt::NoPen);

        for (int i = 0; i < values.size(); ++i)
        {
            if (values.size() > 45 && i % 4 != 0 && i != values.size() - 1)
                continue;

            painter.drawEllipse(pointAt(i, area, minValue, maxValue), 3, 3);
        }
    }

    void drawBars(QPainter &painter, const QRect &area, double minValue, double maxValue)
    {
        Q_UNUSED(minValue);

        int count = values.size();

        if (count <= 0 || qFuzzyIsNull(maxValue))
            return;

        double step = static_cast<double>(area.width()) / count;
        double barWidth = qMax(3.0, step * 0.55);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#38bdf8"));

        for (int i = 0; i < count; ++i)
        {
            double normalized = values[i] / maxValue;
            double barHeight = normalized * area.height();

            QRectF bar(
                area.left() + i * step + (step - barWidth) / 2.0,
                area.bottom() - barHeight,
                barWidth,
                barHeight
                );

            painter.drawRoundedRect(bar, 3, 3);
        }
    }

    void drawCandles(QPainter &painter, const QRect &area, double minValue, double maxValue)
    {
        if (candles.isEmpty())
            return;

        int count = candles.size();
        double step = static_cast<double>(area.width()) / count;
        double candleWidth = qMax(4.0, step * 0.52);

        for (int i = 0; i < count; ++i)
        {
            const CandleUiData &candle = candles[i];

            double x = area.left() + i * step + step / 2.0;

            QColor color = candle.close >= candle.open
                               ? QColor("#22c55e")
                               : QColor("#ef4444");

            double highY = mapY(candle.high, area, minValue, maxValue);
            double lowY = mapY(candle.low, area, minValue, maxValue);
            double openY = mapY(candle.open, area, minValue, maxValue);
            double closeY = mapY(candle.close, area, minValue, maxValue);

            painter.setPen(QPen(color, 2));
            painter.drawLine(QPointF(x, highY), QPointF(x, lowY));

            QRectF body(
                x - candleWidth / 2.0,
                qMin(openY, closeY),
                candleWidth,
                qAbs(closeY - openY)
                );

            if (body.height() < 3)
                body.setHeight(3);

            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRoundedRect(body, 3, 3);
        }
    }

    void drawSupportResistance(QPainter &painter, const QRect &area, double minValue, double maxValue)
    {
        if (supportLevels.isEmpty() && resistanceLevels.isEmpty())
            return;

        painter.setFont(QFont("Segoe UI", 8, QFont::Bold));

        for (int i = 0; i < supportLevels.size(); ++i)
        {
            double level = supportLevels[i];

            if (level < minValue || level > maxValue)
                continue;

            double y = mapY(level, area, minValue, maxValue);

            painter.setPen(QPen(QColor("#22c55e"), 1, Qt::DashLine));
            painter.drawLine(area.left(), y, area.right(), y);

            QRect labelRect(area.right() - 90, static_cast<int>(y) - 12, 84, 22);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(34, 197, 94, 40));
            painter.drawRoundedRect(labelRect, 7, 7);

            painter.setPen(QColor("#86efac"));
            painter.drawText(labelRect, Qt::AlignCenter,
                             "S" + QString::number(i + 1) + " " + formatChartValue(level));
        }

        for (int i = 0; i < resistanceLevels.size(); ++i)
        {
            double level = resistanceLevels[i];

            if (level < minValue || level > maxValue)
                continue;

            double y = mapY(level, area, minValue, maxValue);

            painter.setPen(QPen(QColor("#ef4444"), 1, Qt::DashLine));
            painter.drawLine(area.left(), y, area.right(), y);

            QRect labelRect(area.right() - 90, static_cast<int>(y) - 12, 84, 22);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(239, 68, 68, 40));
            painter.drawRoundedRect(labelRect, 7, 7);

            painter.setPen(QColor("#fecaca"));
            painter.drawText(labelRect, Qt::AlignCenter,
                             "R" + QString::number(i + 1) + " " + formatChartValue(level));
        }
    }

    void drawRsi(QPainter &painter, const QRect &area)
    {
        painter.setPen(QPen(QColor("#ef4444"), 1, Qt::DashLine));
        int level70 = area.bottom() - area.height() * 70 / 100;
        painter.drawLine(area.left(), level70, area.right(), level70);

        painter.setPen(QPen(QColor("#22c55e"), 1, Qt::DashLine));
        int level30 = area.bottom() - area.height() * 30 / 100;
        painter.drawLine(area.left(), level30, area.right(), level30);

        painter.setPen(QColor("#8f9bb3"));
        painter.drawText(area.left() + 4, level70 - 4, "70");
        painter.drawText(area.left() + 4, level30 - 4, "30");

        QPainterPath path;

        auto rsiPoint = [&](int index)
        {
            double x = area.left() + static_cast<double>(area.width()) * index / (values.size() - 1);
            double y = area.bottom() - values[index] / 100.0 * area.height();
            return QPointF(x, y);
        };

        path.moveTo(rsiPoint(0));

        for (int i = 1; i < values.size(); ++i)
            path.lineTo(rsiPoint(i));

        painter.setPen(QPen(QColor("#a855f7"), 3));
        painter.drawPath(path);

        painter.setBrush(QColor("#a855f7"));
        painter.setPen(Qt::NoPen);

        for (int i = 0; i < values.size(); ++i)
        {
            if (values.size() > 45 && i % 4 != 0 && i != values.size() - 1)
                continue;

            painter.drawEllipse(rsiPoint(i), 3, 3);
        }
    }
};

ai_project::ai_project(QWidget *parent)
    : QMainWindow(parent),
    networkManager(new QNetworkAccessManager(this)),
    autoUpdateTimer(new QTimer(this)),
    coinList(nullptr),
    coinNameLabel(nullptr),
    coinPriceLabel(nullptr),
    coinChangeLabel(nullptr),
    periodInfoLabel(nullptr),
    lastUpdateLabel(nullptr),
    priceChart(nullptr),
    volumeChart(nullptr),
    rsiChart(nullptr),
    period24Button(nullptr),
    period7Button(nullptr),
    period30Button(nullptr),
    period90Button(nullptr),

    lineButton(nullptr),
    candleButton(nullptr),
    levelsButton(nullptr),
    refreshButton(nullptr),
    autoUpdateButton(nullptr),
    alertCoinCombo(nullptr),
    alertConditionCombo(nullptr),
    alertPriceEdit(nullptr),
    alertsList(nullptr),
    fromCoinCombo(nullptr),
    toCoinCombo(nullptr),
    amountEdit(nullptr),
    converterResultLabel(nullptr),
    selectedPeriod("24ч"),
    candleMode(false),
    showLevels(true),
    marketRequestInProgress(false),
    lastMarketRequestMs(0)
{
    setWindowTitle("CryptoTrack");
    resize(1320, 820);

    fillDemoData();
    setupInterface();
    setupStyle();

    autoUpdateTimer->setInterval(120000);

    connect(autoUpdateTimer, &QTimer::timeout, this, [this]()
            {
                fetchMarketData();
            });

    buildCoinList();
    loadAlertsFromFile();
    loadSettingsFromFile();
    refreshAlertsList();

    if (!coins.isEmpty())
    {
        int selectedIndex = findCoinIndexById(selectedCoinId);

        if (selectedIndex < 0)
            selectedIndex = 0;

        coinList->blockSignals(true);
        coinList->setCurrentRow(selectedIndex);
        coinList->blockSignals(false);

        selectCoin(selectedIndex);
    }

    if (autoUpdateButton->isChecked())
    {
        autoUpdateTimer->start();
        autoUpdateButton->setText("Автообновление: вкл");
    }

    fetchMarketData();
}

ai_project::~ai_project()
{
    saveAlertsToFile();
    saveSettingsToFile();
}
void ai_project::setupInterface()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(16);

    QFrame *leftPanel = createCard();
    leftPanel->setFixedWidth(270);

    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 16, 16, 16);
    leftLayout->setSpacing(12);

    QLabel *appTitle = new QLabel("CryptoTrack");
    appTitle->setObjectName("AppTitle");

    QLabel *appSubtitle = new QLabel("Курсы и аналитика криптовалют");
    appSubtitle->setObjectName("MutedText");

    coinList = new QListWidget();
    coinList->setObjectName("CoinList");
    coinList->setSpacing(8);
    coinList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    leftLayout->addWidget(appTitle);
    leftLayout->addWidget(appSubtitle);
    leftLayout->addSpacing(10);
    leftLayout->addWidget(coinList);

    QFrame *centerPanel = createCard();

    QVBoxLayout *centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(22, 22, 22, 22);
    centerLayout->setSpacing(14);

    QHBoxLayout *coinHeaderLayout = new QHBoxLayout();

    QVBoxLayout *coinTextLayout = new QVBoxLayout();
    coinTextLayout->setSpacing(4);

    coinNameLabel = new QLabel("Bitcoin");
    coinNameLabel->setObjectName("CoinName");

    coinPriceLabel = new QLabel("$0");
    coinPriceLabel->setObjectName("CoinPrice");

    coinChangeLabel = new QLabel("+0.00%");
    coinChangeLabel->setObjectName("PositiveText");

    coinTextLayout->addWidget(coinNameLabel);
    coinTextLayout->addWidget(coinPriceLabel);
    coinTextLayout->addWidget(coinChangeLabel);

    QVBoxLayout *updateLayout = new QVBoxLayout();
    updateLayout->setAlignment(Qt::AlignRight | Qt::AlignTop);

    lastUpdateLabel = new QLabel("Последнее обновление: —");
    lastUpdateLabel->setObjectName("MutedText");

    refreshButton = createSmallButton("Обновить");

    autoUpdateButton = createSmallButton("Автообновление: выкл");
    autoUpdateButton->setCheckable(true);

    updateLayout->addWidget(lastUpdateLabel);
    updateLayout->addWidget(refreshButton, 0, Qt::AlignRight);
    updateLayout->addWidget(autoUpdateButton, 0, Qt::AlignRight);

    coinHeaderLayout->addLayout(coinTextLayout);
    coinHeaderLayout->addStretch();
    coinHeaderLayout->addLayout(updateLayout);

    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(10);

    period24Button = createSmallButton("24ч");
    period7Button = createSmallButton("7д");
    period30Button = createSmallButton("30д");
    period90Button = createSmallButton("90д");

    period24Button->setCheckable(true);
    period7Button->setCheckable(true);
    period30Button->setCheckable(true);
    period90Button->setCheckable(true);
    period24Button->setChecked(true);

    QButtonGroup *periodGroup = new QButtonGroup(this);
    periodGroup->addButton(period24Button);
    periodGroup->addButton(period7Button);
    periodGroup->addButton(period30Button);
    periodGroup->addButton(period90Button);
    periodGroup->setExclusive(true);

    lineButton = createSmallButton("Линия");
    candleButton = createSmallButton("Свечи");
    levelsButton = createSmallButton("Уровни");

    lineButton->setCheckable(true);
    candleButton->setCheckable(true);
    levelsButton->setCheckable(true);

    lineButton->setChecked(true);
    levelsButton->setChecked(true);

    QButtonGroup *chartGroup = new QButtonGroup(this);
    chartGroup->addButton(lineButton);
    chartGroup->addButton(candleButton);
    chartGroup->setExclusive(true);

    periodInfoLabel = new QLabel("Период: 24 часа, шаг: 1 час");
    periodInfoLabel->setObjectName("MutedText");

    controlLayout->addWidget(period24Button);
    controlLayout->addWidget(period7Button);
    controlLayout->addWidget(period30Button);
    controlLayout->addWidget(period90Button);
    controlLayout->addSpacing(20);
    controlLayout->addWidget(lineButton);
    controlLayout->addWidget(candleButton);
    controlLayout->addWidget(levelsButton);
    controlLayout->addStretch();
    controlLayout->addWidget(periodInfoLabel);

    QLabel *mainChartTitle = new QLabel("График цены");
    mainChartTitle->setObjectName("BlockTitle");

    priceChart = new MiniChartWidget();
    priceChart->setMinimumHeight(280);

    QHBoxLayout *smallChartsLayout = new QHBoxLayout();
    smallChartsLayout->setSpacing(14);

    QFrame *volumeCard = createCard();
    QVBoxLayout *volumeLayout = new QVBoxLayout(volumeCard);
    volumeLayout->setContentsMargins(14, 14, 14, 14);

    QLabel *volumeTitle = new QLabel("Объём торгов");
    volumeTitle->setObjectName("BlockTitle");

    volumeChart = new MiniChartWidget();
    volumeChart->setChartType(MiniChartWidget::BarChart);
    volumeChart->setMinimumHeight(160);

    volumeLayout->addWidget(volumeTitle);
    volumeLayout->addWidget(volumeChart);

    QFrame *rsiCard = createCard();
    QVBoxLayout *rsiLayout = new QVBoxLayout(rsiCard);
    rsiLayout->setContentsMargins(14, 14, 14, 14);

    QLabel *rsiTitle = new QLabel("RSI");
    rsiTitle->setObjectName("BlockTitle");

    rsiChart = new MiniChartWidget();
    rsiChart->setChartType(MiniChartWidget::RsiChart);
    rsiChart->setMinimumHeight(160);

    rsiLayout->addWidget(rsiTitle);
    rsiLayout->addWidget(rsiChart);

    smallChartsLayout->addWidget(volumeCard);
    smallChartsLayout->addWidget(rsiCard);

    centerLayout->addLayout(coinHeaderLayout);
    centerLayout->addLayout(controlLayout);
    centerLayout->addWidget(mainChartTitle);
    centerLayout->addWidget(priceChart, 1);
    centerLayout->addLayout(smallChartsLayout);

    QFrame *rightPanel = createCard();
    rightPanel->setFixedWidth(310);

    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(16, 16, 16, 16);
    rightLayout->setSpacing(12);

    QLabel *alertsTitle = new QLabel("Оповещения");
    alertsTitle->setObjectName("BlockTitle");

    alertCoinCombo = new QComboBox();
    alertConditionCombo = new QComboBox();
    alertPriceEdit = new QLineEdit();
    QPushButton *addAlertButton = createSmallButton("Добавить оповещение");

    alertConditionCombo->addItem("Цена выше");
    alertConditionCombo->addItem("Цена ниже");
    alertPriceEdit->setPlaceholderText("Целевая цена, USD");

    alertsList = new QListWidget();
    alertsList->setObjectName("SimpleList");
    alertsList->setMinimumHeight(125);

    QPushButton *deleteAlertButton = createSmallButton("Удалить выбранное");
    QPushButton *clearTriggeredAlertsButton = createSmallButton("Очистить сработавшие");

    deleteAlertButton->setMinimumHeight(36);
    clearTriggeredAlertsButton->setMinimumHeight(36);

    QVBoxLayout *alertButtonsLayout = new QVBoxLayout();
    alertButtonsLayout->setSpacing(8);
    alertButtonsLayout->addWidget(deleteAlertButton);
    alertButtonsLayout->addWidget(clearTriggeredAlertsButton);

    QLabel *converterTitle = new QLabel("Конвертор");
    converterTitle->setObjectName("BlockTitle");

    fromCoinCombo = new QComboBox();
    toCoinCombo = new QComboBox();
    amountEdit = new QLineEdit();
    QPushButton *calculateButton = createSmallButton("Рассчитать");

    amountEdit->setPlaceholderText("Количество");
    amountEdit->setText("1");

    converterResultLabel = new QLabel("Результат: —");
    converterResultLabel->setObjectName("ConverterResult");
    converterResultLabel->setWordWrap(true);

    rightLayout->addWidget(alertsTitle);
    rightLayout->addWidget(alertCoinCombo);
    rightLayout->addWidget(alertConditionCombo);
    rightLayout->addWidget(alertPriceEdit);
    rightLayout->addWidget(addAlertButton);
    rightLayout->addWidget(alertsList);
    rightLayout->addLayout(alertButtonsLayout);
    rightLayout->addSpacing(14);

    rightLayout->addWidget(converterTitle);
    rightLayout->addWidget(fromCoinCombo);
    rightLayout->addWidget(toCoinCombo);
    rightLayout->addWidget(amountEdit);
    rightLayout->addWidget(calculateButton);
    rightLayout->addWidget(converterResultLabel);
    rightLayout->addStretch();

    rootLayout->addWidget(leftPanel);
    rootLayout->addWidget(centerPanel, 1);
    rootLayout->addWidget(rightPanel);

    connect(coinList, &QListWidget::currentRowChanged, this, &ai_project::selectCoin);

    connect(refreshButton, &QPushButton::clicked, this, &ai_project::refreshDemoData);

    connect(autoUpdateButton, &QPushButton::clicked, this, &ai_project::toggleAutoUpdate);

    connect(period24Button, &QPushButton::clicked, this, [this]()
            {
                selectedPeriod = "24ч";
                periodInfoLabel->setText("Период: 24 часа, шаг: 1 час");
                selectCoin(coinList->currentRow());
                saveSettingsToFile();
            });

    connect(period7Button, &QPushButton::clicked, this, [this]()
            {
                selectedPeriod = "7д";
                periodInfoLabel->setText("Период: 7 дней, шаг: 4 часа");
                selectCoin(coinList->currentRow());
                saveSettingsToFile();
            });

    connect(period30Button, &QPushButton::clicked, this, [this]()
            {
                selectedPeriod = "30д";
                periodInfoLabel->setText("Период: 30 дней, шаг: 1 день");
                selectCoin(coinList->currentRow());
                saveSettingsToFile();
            });

    connect(period90Button, &QPushButton::clicked, this, [this]()
            {
                selectedPeriod = "90д";
                periodInfoLabel->setText("Период: 90 дней, шаг: 1 день");
                selectCoin(coinList->currentRow());
                saveSettingsToFile();
            });

    connect(lineButton, &QPushButton::clicked, this, [this]()
            {
                candleMode = false;
                priceChart->setChartType(MiniChartWidget::LineChart);
                saveSettingsToFile();
            });

    connect(candleButton, &QPushButton::clicked, this, [this]()
            {
                candleMode = true;
                priceChart->setChartType(MiniChartWidget::CandleChart);
                saveSettingsToFile();
            });

    connect(levelsButton, &QPushButton::clicked, this, [this]()
            {
                showLevels = levelsButton->isChecked();

                int currentRow = coinList->currentRow();

                if (currentRow >= 0)
                    selectCoin(currentRow);

                saveSettingsToFile();
            });
    connect(addAlertButton, &QPushButton::clicked, this, &ai_project::addAlert);

    connect(deleteAlertButton, &QPushButton::clicked, this, &ai_project::deleteSelectedAlert);
    connect(clearTriggeredAlertsButton, &QPushButton::clicked, this, &ai_project::clearTriggeredAlerts);

    connect(calculateButton, &QPushButton::clicked, this, &ai_project::updateConverter);

    connect(fromCoinCombo, &QComboBox::currentTextChanged, this, &ai_project::updateConverter);
    connect(toCoinCombo, &QComboBox::currentTextChanged, this, &ai_project::updateConverter);
    connect(amountEdit, &QLineEdit::textChanged, this, &ai_project::updateConverter);
}

void ai_project::setupStyle()
{
    qApp->setStyleSheet(
        "QMainWindow {"
        "    background: #070b16;"
        "}"

        "QWidget {"
        "    font-family: Segoe UI, Arial;"
        "    color: #eef2ff;"
        "    font-size: 14px;"
        "}"

        "QFrame#Card {"
        "    background: #0f172a;"
        "    border: 1px solid #1e293b;"
        "    border-radius: 22px;"
        "}"

        "QLabel#AppTitle {"
        "    font-size: 28px;"
        "    font-weight: 800;"
        "    color: #ffffff;"
        "}"

        "QLabel#CoinName {"
        "    font-size: 24px;"
        "    font-weight: 700;"
        "    color: #ffffff;"
        "}"

        "QLabel#CoinPrice {"
        "    font-size: 42px;"
        "    font-weight: 800;"
        "    color: #ffffff;"
        "}"

        "QLabel#BlockTitle {"
        "    font-size: 17px;"
        "    font-weight: 700;"
        "    color: #f8fafc;"
        "}"

        "QLabel#MutedText {"
        "    color: #94a3b8;"
        "    font-size: 13px;"
        "}"

        "QLabel#PositiveText {"
        "    color: #22c55e;"
        "    font-size: 16px;"
        "    font-weight: 700;"
        "}"

        "QLabel#NegativeText {"
        "    color: #ef4444;"
        "    font-size: 16px;"
        "    font-weight: 700;"
        "}"

        "QLabel#ConverterResult {"
        "    background: #111c33;"
        "    border: 1px solid #27344f;"
        "    border-radius: 14px;"
        "    padding: 12px;"
        "    color: #dbeafe;"
        "    font-weight: 700;"
        "}"

        "QPushButton {"
        "    background: #1e293b;"
        "    color: #e5e7eb;"
        "    border: 1px solid #334155;"
        "    border-radius: 13px;"
        "    padding: 9px 14px;"
        "    font-weight: 600;"
        "}"

        "QPushButton:hover {"
        "    background: #273449;"
        "    border: 1px solid #475569;"
        "}"

        "QPushButton:checked {"
        "    background: #2563eb;"
        "    color: white;"
        "    border: 1px solid #3b82f6;"
        "}"

        "QListWidget#CoinList, QListWidget#SimpleList {"
        "    background: transparent;"
        "    border: none;"
        "    outline: none;"
        "}"

        "QListWidget::item {"
        "    border-radius: 16px;"
        "    margin: 3px;"
        "    padding: 4px;"
        "}"

        "QListWidget::item:selected {"
        "    background: #1d4ed8;"
        "}"

        "QComboBox, QLineEdit {"
        "    background: #111827;"
        "    border: 1px solid #334155;"
        "    border-radius: 13px;"
        "    padding: 9px 11px;"
        "    color: #f8fafc;"
        "    selection-background-color: #2563eb;"
        "    selection-color: #ffffff;"
        "}"

        "QComboBox:hover, QLineEdit:hover {"
        "    border: 1px solid #475569;"
        "}"

        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 28px;"
        "}"

        "QComboBox QAbstractItemView {"
        "    background: #111827;"
        "    color: #f8fafc;"
        "    border: 1px solid #334155;"
        "    selection-background-color: #2563eb;"
        "    selection-color: #ffffff;"
        "    outline: none;"
        "}"

        "QMessageBox {"
        "    background-color: #0f172a;"
        "}"

        "QMessageBox QLabel {"
        "    color: #f8fafc;"
        "    font-size: 14px;"
        "}"

        "QMessageBox QPushButton {"
        "    min-width: 80px;"
        "    background: #1e293b;"
        "    color: #ffffff;"
        "    border: 1px solid #334155;"
        "    border-radius: 12px;"
        "    padding: 8px 14px;"
        "}"

        "QMessageBox QPushButton:hover {"
        "    background: #2563eb;"
        "}"

        "QScrollBar:vertical {"
        "    background: transparent;"
        "    width: 8px;"
        "}"

        "QScrollBar::handle:vertical {"
        "    background: #334155;"
        "    border-radius: 4px;"
        "}"

        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}"
        );
}

void ai_project::fillDemoData()
{
    coins.clear();

    struct BaseCoin
    {
        QString id;
        QString symbol;
        QString name;
        double price;
        double change;
        double volume;
    };

    QVector<BaseCoin> baseCoins = {
        {"bitcoin", "BTC", "Bitcoin", 68420.0, 2.41, 28500000000.0},
        {"ethereum", "ETH", "Ethereum", 3570.0, 1.76, 15900000000.0},
        {"solana", "SOL", "Solana", 168.4, -0.82, 4200000000.0},
        {"dogecoin", "DOGE", "Dogecoin", 0.147, 4.15, 2100000000.0},
        {"cardano", "ADA", "Cardano", 0.46, -1.24, 860000000.0},
        {"ripple", "XRP", "Ripple", 0.53, 0.64, 1300000000.0},
        {"polkadot", "DOT", "Polkadot", 7.12, -2.35, 510000000.0}
    };

    for (int coinIndex = 0; coinIndex < baseCoins.size(); ++coinIndex)
    {
        const BaseCoin &base = baseCoins[coinIndex];

        CoinUiData coin;
        coin.id = base.id;
        coin.symbol = base.symbol;
        coin.name = base.name;
        coin.price = base.price;
        coin.changePercent = base.change;
        coin.volume = base.volume;

        double start = base.price * (1.0 - base.change / 100.0);

        for (int i = 0; i < 60; ++i)
        {
            double t = static_cast<double>(i) / 59.0;
            double wave = qSin(i * 0.35 + coinIndex) * base.price * 0.018;
            double trend = (base.price - start) * t;
            double localNoise = qSin(i * 0.91 + coinIndex * 2.0) * base.price * 0.006;

            coin.history.append(start + trend + wave + localNoise);

            double volumeWave = qAbs(qSin(i * 0.27 + coinIndex));
            coin.volumes.append(base.volume * (0.45 + volumeWave * 0.55));

            double rsiValue = 50.0 + qSin(i * 0.22 + coinIndex) * 22.0 + base.change * 1.5;
            rsiValue = qBound(12.0, rsiValue, 88.0);
            coin.rsi.append(rsiValue);
        }

        for (int i = 0; i < coin.history.size(); ++i)
        {
            double open = i == 0 ? coin.history[i] * 0.998 : coin.history[i - 1];
            double close = coin.history[i];

            CandleUiData candle;
            candle.open = open;
            candle.close = close;
            candle.high = qMax(open, close) * 1.004;
            candle.low = qMin(open, close) * 0.996;

            coin.candles.append(candle);
        }

        coins.append(coin);
    }
}

void ai_project::buildCoinList()
{
    coinList->clear();

    alertCoinCombo->clear();
    fromCoinCombo->clear();
    toCoinCombo->clear();

    for (const CoinUiData &coin : coins)
    {
        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole, coin.id);
        item->setSizeHint(QSize(220, 72));

        QWidget *rowWidget = new QWidget();
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(10, 8, 10, 8);
        rowLayout->setSpacing(8);

        QLabel *symbolLabel = new QLabel(coin.symbol);
        symbolLabel->setFixedWidth(48);
        symbolLabel->setAlignment(Qt::AlignCenter);
        symbolLabel->setStyleSheet(
            "background:#172554;"
            "border-radius:14px;"
            "padding:10px 6px;"
            "font-weight:800;"
            "color:#dbeafe;"
            );

        QVBoxLayout *textLayout = new QVBoxLayout();
        textLayout->setSpacing(2);

        QLabel *nameLabel = new QLabel(coin.name);
        nameLabel->setStyleSheet("font-weight:700; color:#f8fafc;");

        QLabel *priceLabel = new QLabel(formatPrice(coin.price));
        priceLabel->setStyleSheet("color:#94a3b8; font-size:12px;");

        textLayout->addWidget(nameLabel);
        textLayout->addWidget(priceLabel);

        QLabel *changeLabel = new QLabel(QString("%1%2%")
                                             .arg(coin.changePercent >= 0 ? "▲ +" : "▼ ")
                                             .arg(coin.changePercent, 0, 'f', 2));

        changeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        changeLabel->setStyleSheet(coin.changePercent >= 0
                                       ? "color:#22c55e; font-weight:800;"
                                       : "color:#ef4444; font-weight:800;");

        rowLayout->addWidget(symbolLabel);
        rowLayout->addLayout(textLayout);
        rowLayout->addStretch();
        rowLayout->addWidget(changeLabel);

        coinList->addItem(item);
        coinList->setItemWidget(item, rowWidget);

        alertCoinCombo->addItem(coin.symbol, coin.id);
        fromCoinCombo->addItem(coin.symbol, coin.id);
        toCoinCombo->addItem(coin.symbol, coin.id);
    }

    if (toCoinCombo->count() > 1)
        toCoinCombo->setCurrentIndex(1);
}

QFrame *ai_project::createCard()
{
    QFrame *frame = new QFrame();
    frame->setObjectName("Card");
    return frame;
}

QPushButton *ai_project::createSmallButton(const QString &text)
{
    QPushButton *button = new QPushButton(text);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

void ai_project::selectCoin(int row)
{
    if (row < 0 || row >= coins.size())
        return;

    selectedCoinId = coins[row].id;

    updateCenterPanel(coins[row]);
    fetchHistoryData(selectedCoinId, selectedPeriod);
    saveSettingsToFile();
}

void ai_project::updateCenterPanel(const CoinUiData &coin)
{
    coinNameLabel->setText(coin.name + " (" + coin.symbol + ")");
    coinPriceLabel->setText(formatPrice(coin.price));

    QString changeText = QString("%1%2% за 24 часа")
                             .arg(coin.changePercent >= 0 ? "▲ +" : "▼ ")
                             .arg(coin.changePercent, 0, 'f', 2);

    coinChangeLabel->setText(changeText);
    coinChangeLabel->setObjectName(coin.changePercent >= 0 ? "PositiveText" : "NegativeText");
    coinChangeLabel->style()->unpolish(coinChangeLabel);
    coinChangeLabel->style()->polish(coinChangeLabel);

    QVector<double> chartValues = coin.history;
    QVector<double> volumeValues = coin.volumes;
    QVector<double> rsiValues = coin.rsi;
    QVector<CandleUiData> candleValues = coin.candles;

    int targetCount = targetPointCountForPeriod(selectedPeriod);

    if (chartValues.size() > targetCount)
        chartValues = chartValues.mid(chartValues.size() - targetCount);

    if (volumeValues.size() > targetCount)
        volumeValues = volumeValues.mid(volumeValues.size() - targetCount);

    if (rsiValues.size() > targetCount)
        rsiValues = rsiValues.mid(rsiValues.size() - targetCount);

    if (candleValues.size() > targetCount)
        candleValues = candleValues.mid(candleValues.size() - targetCount);

    QVector<double> supportLevels;
    QVector<double> resistanceLevels;

    if (showLevels)
    {
        supportLevels = calculateSupportLevels(chartValues);
        resistanceLevels = calculateResistanceLevels(chartValues);
    }

    priceChart->setValues(chartValues);
    priceChart->setCandles(candleValues);
    priceChart->setSupportResistanceLevels(supportLevels, resistanceLevels);

    volumeChart->setValues(volumeValues);
    rsiChart->setValues(rsiValues);

    if (candleMode)
        priceChart->setChartType(MiniChartWidget::CandleChart);
    else
        priceChart->setChartType(MiniChartWidget::LineChart);

    lastUpdateLabel->setText("Последнее обновление: " +
                             QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss"));
}

void ai_project::updateConverter()
{
    if (!fromCoinCombo || !toCoinCombo || !amountEdit || !converterResultLabel)
        return;

    QString fromId = fromCoinCombo->currentData().toString();
    QString toId = toCoinCombo->currentData().toString();

    int fromIndex = findCoinIndexById(fromId);
    int toIndex = findCoinIndexById(toId);

    if (fromIndex < 0 || toIndex < 0)
        return;

    bool ok = false;
    double amount = amountEdit->text().replace(",", ".").toDouble(&ok);

    if (!ok || amount <= 0.0)
    {
        converterResultLabel->setText("Результат: введите корректное количество");
        return;
    }

    double result = amount * coins[fromIndex].price / coins[toIndex].price;

    converterResultLabel->setText(
        QString("Результат: %1 %2 = %3 %4")
            .arg(amount, 0, 'f', 4)
            .arg(coins[fromIndex].symbol)
            .arg(result, 0, 'f', 6)
            .arg(coins[toIndex].symbol)
        );
}

void ai_project::addAlert()
{
    QString coinId = alertCoinCombo->currentData().toString();
    QString coinSymbol = alertCoinCombo->currentText();

    bool ok = false;
    double targetPrice = alertPriceEdit->text().replace(",", ".").toDouble(&ok);

    if (!ok || targetPrice <= 0.0)
    {
        QMessageBox::warning(this, "Ошибка", "Введите корректную целевую цену.");
        return;
    }

    PriceAlertUiData alert;
    alert.coinId = coinId;
    alert.coinSymbol = coinSymbol;
    alert.targetPrice = targetPrice;
    alert.aboveCondition = alertConditionCombo->currentIndex() == 0;
    alert.active = true;

    priceAlerts.append(alert);

    alertPriceEdit->clear();
    refreshAlertsList();
    saveAlertsToFile();

    QMessageBox::information(
        this,
        "Оповещение добавлено",
        "Оповещение успешно добавлено:\n\n" + alertText(alert)
        );
}

void ai_project::refreshDemoData()
{
    fetchMarketData();
}

int ai_project::findCoinIndexById(const QString &id) const
{
    for (int i = 0; i < coins.size(); ++i)
    {
        if (coins[i].id == id)
            return i;
    }

    return -1;
}

QString ai_project::formatPrice(double value) const
{
    if (value >= 1000.0)
        return "$" + QString::number(value, 'f', 0);

    if (value >= 1.0)
        return "$" + QString::number(value, 'f', 2);

    return "$" + QString::number(value, 'f', 4);
}

QString ai_project::formatVolume(double value) const
{
    if (value >= 1000000000.0)
        return QString::number(value / 1000000000.0, 'f', 2) + " млрд USD";

    if (value >= 1000000.0)
        return QString::number(value / 1000000.0, 'f', 2) + " млн USD";

    return QString::number(value, 'f', 2) + " USD";
}

void ai_project::fetchMarketData()
{
    if (marketRequestInProgress)
        return;

    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    if (lastMarketRequestMs > 0 && nowMs - lastMarketRequestMs < 15000)
    {
        lastUpdateLabel->setText(
            "Слишком частое обновление. Подождите несколько секунд."
            );
        return;
    }

    marketRequestInProgress = true;
    lastMarketRequestMs = nowMs;

    refreshButton->setEnabled(false);
    refreshButton->setText("Загрузка...");

    QUrl url(
        "https://api.coingecko.com/api/v3/coins/markets"
        "?vs_currency=usd"
        "&ids=bitcoin,ethereum,solana,dogecoin,cardano,ripple,polkadot"
        "&order=market_cap_desc"
        "&per_page=7"
        "&page=1"
        "&sparkline=false"
        "&price_change_percentage=24h"
        );

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "CryptoTrackQtCourseProject/1.0");

    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                handleMarketDataReply(reply);
                reply->deleteLater();
            });
}

void ai_project::handleMarketDataReply(QNetworkReply *reply)
{
    marketRequestInProgress = false;

    refreshButton->setEnabled(true);
    refreshButton->setText("Обновить");

    if (reply->error() != QNetworkReply::NoError)
    {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (statusCode == 429)
        {
            lastUpdateLabel->setText(
                "CoinGecko временно ограничил запросы. Подождите 1–2 минуты."
                );

            QMessageBox::information(
                this,
                "Лимит запросов",
                "CoinGecko временно ограничил количество запросов.\n\n"
                "Это не ошибка программы. Подождите 1–2 минуты и нажмите обновить снова.\n"
                "Чтобы не ловить лимит, не нажимайте обновление слишком часто."
                );

            return;
        }

        int currentRow = coinList->currentRow();

        if (currentRow >= 0 && currentRow < coins.size())
            updateCenterPanel(coins[currentRow]);

        lastUpdateLabel->setText(
            "Не удалось обновить данные. Используются последние загруженные значения."
            );

        return;
    }

    QByteArray responseData = reply->readAll();

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(responseData, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isArray())
    {
        lastUpdateLabel->setText("Ошибка разбора данных API");

        QMessageBox::warning(
            this,
            "Ошибка данных",
            "CoinGecko вернул данные в неожиданном формате.\n\n"
            "Ошибка JSON: " + parseError.errorString()
            );

        return;
    }

    QJsonArray array = document.array();

    for (const QJsonValue &value : array)
    {
        if (value.isObject())
            updateCoinFromJson(value.toObject());
    }

    QString oldSelectedId = selectedCoinId;

    buildCoinList();

    int selectedIndex = findCoinIndexById(oldSelectedId);

    if (selectedIndex < 0)
        selectedIndex = 0;

    coinList->blockSignals(true);
    coinList->setCurrentRow(selectedIndex);
    coinList->blockSignals(false);

    selectedCoinId = coins[selectedIndex].id;
    updateCenterPanel(coins[selectedIndex]);
    updateConverter();

    lastUpdateLabel->setText(
        "Последнее обновление цен: " +
        QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss")
        );

    checkPriceAlerts();
}

void ai_project::updateCoinFromJson(const QJsonObject &object)
{
    QString id = object.value("id").toString();

    int index = findCoinIndexById(id);

    if (index < 0)
        return;

    double newPrice = object.value("current_price").toDouble(coins[index].price);
    double newChange = object.value("price_change_percentage_24h").toDouble(coins[index].changePercent);
    double newVolume = object.value("total_volume").toDouble(coins[index].volume);

    coins[index].price = newPrice;
    coins[index].changePercent = newChange;
    coins[index].volume = newVolume;

    if (!coins[index].history.isEmpty())
    {
        coins[index].history.removeFirst();
        coins[index].history.append(newPrice);
    }
    if (!coins[index].candles.isEmpty())
    {
        double open = coins[index].candles.last().close;

        CandleUiData candle;
        candle.open = open;
        candle.close = newPrice;
        candle.high = qMax(open, newPrice) * 1.002;
        candle.low = qMin(open, newPrice) * 0.998;

        coins[index].candles.removeFirst();
        coins[index].candles.append(candle);
    }

    if (!coins[index].volumes.isEmpty())
    {
        coins[index].volumes.removeFirst();
        coins[index].volumes.append(newVolume);
    }

    if (!coins[index].rsi.isEmpty())
    {
        double direction = coins[index].changePercent >= 0 ? 1.0 : -1.0;
        double newRsi = qBound(10.0, coins[index].rsi.last() + direction * 1.2, 90.0);

        coins[index].rsi.removeFirst();
        coins[index].rsi.append(newRsi);
    }
}

void ai_project::fetchHistoryData(const QString &coinId, const QString &period)
{
    int days = daysForPeriod(period);

    lastUpdateLabel->setText("Загрузка истории курса...");

    QString urlText =
        "https://api.coingecko.com/api/v3/coins/" + coinId +
        "/market_chart?vs_currency=usd&days=" + QString::number(days);

    QNetworkRequest request{QUrl(urlText)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "CryptoTrackQtCourseProject/1.0");

    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, coinId, period]()
            {
                handleHistoryDataReply(reply, coinId, period);
                reply->deleteLater();
            });
}

void ai_project::handleHistoryDataReply(QNetworkReply *reply, const QString &coinId, const QString &period)
{
    if (coinId != selectedCoinId || period != selectedPeriod)
        return;

    if (reply->error() != QNetworkReply::NoError)
    {
        lastUpdateLabel->setText("История курса не загружена, показаны сохранённые данные");
        return;
    }

    QByteArray responseData = reply->readAll();

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(responseData, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        lastUpdateLabel->setText("Ошибка разбора истории курса");
        return;
    }

    QJsonObject root = document.object();

    QJsonArray pricesArray = root.value("prices").toArray();
    QJsonArray volumesArray = root.value("total_volumes").toArray();

    QVector<double> rawPrices;
    QVector<double> rawVolumes;

    for (const QJsonValue &value : pricesArray)
    {
        QJsonArray pair = value.toArray();

        if (pair.size() >= 2)
            rawPrices.append(pair.at(1).toDouble());
    }

    for (const QJsonValue &value : volumesArray)
    {
        QJsonArray pair = value.toArray();

        if (pair.size() >= 2)
            rawVolumes.append(pair.at(1).toDouble());
    }

    if (rawPrices.size() < 2)
    {
        lastUpdateLabel->setText("Недостаточно исторических данных");
        return;
    }

    int coinIndex = findCoinIndexById(coinId);

    if (coinIndex < 0)
        return;

    int targetCount = targetPointCountForPeriod(period);

    QVector<CandleUiData> preparedCandles = buildCandlesFromPrices(rawPrices, targetCount);
    QVector<double> preparedPrices = closeValuesFromCandles(preparedCandles);
    QVector<double> preparedVolumes = reduceToAverageValues(rawVolumes, targetCount);

    if (preparedPrices.isEmpty())
        preparedPrices = reduceToClosingValues(rawPrices, targetCount);

    coins[coinIndex].candles = preparedCandles;
    coins[coinIndex].history = preparedPrices;
    coins[coinIndex].volumes = preparedVolumes;
    coins[coinIndex].rsi = calculateRsiValues(preparedPrices);

    if (!preparedPrices.isEmpty())
        coins[coinIndex].price = preparedPrices.last();

    updateCenterPanel(coins[coinIndex]);

    lastUpdateLabel->setText(
        "История обновлена: " +
        QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss")
        );
}

int ai_project::daysForPeriod(const QString &period) const
{
    if (period == "24ч")
        return 1;

    if (period == "7д")
        return 7;

    if (period == "30д")
        return 30;

    return 90;
}

int ai_project::targetPointCountForPeriod(const QString &period) const
{
    if (period == "24ч")
        return 24;

    if (period == "7д")
        return 42;

    if (period == "30д")
        return 30;

    return 90;
}

QVector<double> ai_project::reduceToClosingValues(const QVector<double> &source, int targetCount) const
{
    QVector<double> result;

    if (source.isEmpty() || targetCount <= 0)
        return result;

    if (source.size() <= targetCount)
        return source;

    for (int i = 0; i < targetCount; ++i)
    {
        int start = static_cast<int>(static_cast<double>(i) * source.size() / targetCount);
        int end = static_cast<int>(static_cast<double>(i + 1) * source.size() / targetCount);

        if (end <= start)
            end = start + 1;

        if (end > source.size())
            end = source.size();

        result.append(source[end - 1]);
    }

    return result;
}

QVector<double> ai_project::reduceToAverageValues(const QVector<double> &source, int targetCount) const
{
    QVector<double> result;

    if (source.isEmpty() || targetCount <= 0)
        return result;

    if (source.size() <= targetCount)
        return source;

    for (int i = 0; i < targetCount; ++i)
    {
        int start = static_cast<int>(static_cast<double>(i) * source.size() / targetCount);
        int end = static_cast<int>(static_cast<double>(i + 1) * source.size() / targetCount);

        if (end <= start)
            end = start + 1;

        if (end > source.size())
            end = source.size();

        double sum = 0.0;
        int count = 0;

        for (int j = start; j < end; ++j)
        {
            sum += source[j];
            ++count;
        }

        if (count > 0)
            result.append(sum / count);
    }

    return result;
}
QVector<CandleUiData> ai_project::buildCandlesFromPrices(const QVector<double> &source, int targetCount) const
{
    QVector<CandleUiData> result;

    if (source.isEmpty() || targetCount <= 0)
        return result;

    if (source.size() < targetCount)
        targetCount = source.size();

    for (int i = 0; i < targetCount; ++i)
    {
        int start = static_cast<int>(static_cast<double>(i) * source.size() / targetCount);
        int end = static_cast<int>(static_cast<double>(i + 1) * source.size() / targetCount);

        if (end <= start)
            end = start + 1;

        if (end > source.size())
            end = source.size();

        CandleUiData candle;
        candle.open = source[start];
        candle.close = source[end - 1];
        candle.high = source[start];
        candle.low = source[start];

        for (int j = start; j < end; ++j)
        {
            candle.high = qMax(candle.high, source[j]);
            candle.low = qMin(candle.low, source[j]);
        }

        result.append(candle);
    }

    return result;
}

QVector<double> ai_project::closeValuesFromCandles(const QVector<CandleUiData> &candles) const
{
    QVector<double> result;

    for (const CandleUiData &candle : candles)
        result.append(candle.close);

    return result;
}

QVector<double> ai_project::calculateSupportLevels(const QVector<double> &prices) const
{
    QVector<double> levels;

    if (prices.size() < 5)
        return levels;

    for (int i = 2; i < prices.size() - 2; ++i)
    {
        bool isLocalMinimum =
            prices[i] < prices[i - 1] &&
            prices[i] < prices[i - 2] &&
            prices[i] <= prices[i + 1] &&
            prices[i] <= prices[i + 2];

        if (isLocalMinimum)
            levels.append(prices[i]);
    }

    if (levels.isEmpty())
        return levels;

    std::sort(levels.begin(), levels.end());

    QVector<double> compactLevels;

    for (double level : levels)
    {
        bool tooClose = false;

        for (double existing : compactLevels)
        {
            double differencePercent = qAbs(level - existing) / existing * 100.0;

            if (differencePercent < 0.8)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
            compactLevels.append(level);
    }

    while (compactLevels.size() > 2)
        compactLevels.removeFirst();

    return compactLevels;
}

QVector<double> ai_project::calculateResistanceLevels(const QVector<double> &prices) const
{
    QVector<double> levels;

    if (prices.size() < 5)
        return levels;

    for (int i = 2; i < prices.size() - 2; ++i)
    {
        bool isLocalMaximum =
            prices[i] > prices[i - 1] &&
            prices[i] > prices[i - 2] &&
            prices[i] >= prices[i + 1] &&
            prices[i] >= prices[i + 2];

        if (isLocalMaximum)
            levels.append(prices[i]);
    }

    if (levels.isEmpty())
        return levels;

    std::sort(levels.begin(), levels.end());

    QVector<double> compactLevels;

    for (double level : levels)
    {
        bool tooClose = false;

        for (double existing : compactLevels)
        {
            double differencePercent = qAbs(level - existing) / existing * 100.0;

            if (differencePercent < 0.8)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
            compactLevels.append(level);
    }

    while (compactLevels.size() > 2)
        compactLevels.removeLast();

    return compactLevels;
}

QVector<double> ai_project::calculateRsiValues(const QVector<double> &prices) const
{
    QVector<double> rsi;

    const int period = 14;

    if (prices.size() < 2)
        return rsi;

    for (int i = 0; i < prices.size(); ++i)
    {
        if (i < period)
        {
            rsi.append(50.0);
            continue;
        }

        double gainSum = 0.0;
        double lossSum = 0.0;

        for (int j = i - period + 1; j <= i; ++j)
        {
            double change = prices[j] - prices[j - 1];

            if (change > 0)
                gainSum += change;
            else
                lossSum += qAbs(change);
        }

        double averageGain = gainSum / period;
        double averageLoss = lossSum / period;

        double value = 50.0;

        if (qFuzzyIsNull(averageLoss) && qFuzzyIsNull(averageGain))
        {
            value = 50.0;
        }
        else if (qFuzzyIsNull(averageLoss))
        {
            value = 100.0;
        }
        else
        {
            double rs = averageGain / averageLoss;
            value = 100.0 - 100.0 / (1.0 + rs);
        }

        rsi.append(qBound(0.0, value, 100.0));
    }

    return rsi;
}

QString ai_project::alertText(const PriceAlertUiData &alert) const
{
    QString conditionText = alert.aboveCondition ? "выше" : "ниже";
    QString statusText = alert.active ? "активно" : "сработало";

    return QString("%1: цена %2 %3 USD [%4]")
        .arg(alert.coinSymbol)
        .arg(conditionText)
        .arg(alert.targetPrice, 0, 'f', 2)
        .arg(statusText);
}

void ai_project::refreshAlertsList()
{
    alertsList->clear();

    if (priceAlerts.isEmpty())
    {
        QListWidgetItem *emptyItem = new QListWidgetItem("Оповещений пока нет");
        emptyItem->setForeground(QColor("#64748b"));
        emptyItem->setFlags(Qt::NoItemFlags);

        alertsList->addItem(emptyItem);
        return;
    }

    for (int i = 0; i < priceAlerts.size(); ++i)
    {
        const PriceAlertUiData &alert = priceAlerts[i];

        QListWidgetItem *item = new QListWidgetItem(
            QString::number(i + 1) + ". " + alertText(alert)
            );

        item->setData(Qt::UserRole, i);

        if (alert.active)
        {
            item->setForeground(QColor("#e5e7eb"));
        }
        else
        {
            item->setForeground(QColor("#64748b"));
        }

        alertsList->addItem(item);
    }
}

void ai_project::checkPriceAlerts()
{
    QStringList triggeredAlerts;

    for (PriceAlertUiData &alert : priceAlerts)
    {
        if (!alert.active)
            continue;

        int coinIndex = findCoinIndexById(alert.coinId);

        if (coinIndex < 0)
            continue;

        double currentPrice = coins[coinIndex].price;

        bool triggered = false;

        if (alert.aboveCondition && currentPrice >= alert.targetPrice)
            triggered = true;

        if (!alert.aboveCondition && currentPrice <= alert.targetPrice)
            triggered = true;

        if (triggered)
        {
            alert.active = false;

            QString conditionText = alert.aboveCondition ? "выше" : "ниже";

            triggeredAlerts.append(
                QString("%1: текущая цена %2 USD, условие: цена %3 %4 USD")
                    .arg(alert.coinSymbol)
                    .arg(currentPrice, 0, 'f', 4)
                    .arg(conditionText)
                    .arg(alert.targetPrice, 0, 'f', 4)
                );
        }
    }

    refreshAlertsList();

    if (!triggeredAlerts.isEmpty())
    {
        saveAlertsToFile();

        QMessageBox::information(
            this,
            "Сработало оповещение",
            "Достигнут заданный уровень цены:\n\n" +
                triggeredAlerts.join("\n")
            );
    }
}

void ai_project::toggleAutoUpdate()
{
    if (autoUpdateButton->isChecked())
    {
        autoUpdateTimer->start();
        autoUpdateButton->setText("Автообновление: вкл");

        lastUpdateLabel->setText(
            "Автообновление включено: каждые 120 секунд"
            );

        fetchMarketData();
    }
    else
    {
        autoUpdateTimer->stop();
        autoUpdateButton->setText("Автообновление: выкл");

        lastUpdateLabel->setText(
            "Автообновление выключено"
            );
    }

    saveSettingsToFile();
}

void ai_project::deleteSelectedAlert()
{
    if (priceAlerts.isEmpty())
    {
        QMessageBox::information(
            this,
            "Удаление оповещения",
            "Список оповещений пуст."
            );
        return;
    }

    int row = alertsList->currentRow();

    if (row < 0 || row >= priceAlerts.size())
    {
        QMessageBox::information(
            this,
            "Удаление оповещения",
            "Выберите оповещение в списке."
            );
        return;
    }

    PriceAlertUiData removedAlert = priceAlerts[row];

    QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        "Удаление оповещения",
        "Удалить выбранное оповещение?\n\n" + alertText(removedAlert),
        QMessageBox::Yes | QMessageBox::No
        );

    if (answer != QMessageBox::Yes)
        return;

    priceAlerts.removeAt(row);
    refreshAlertsList();
    saveAlertsToFile();
}

void ai_project::clearTriggeredAlerts()
{
    if (priceAlerts.isEmpty())
    {
        QMessageBox::information(
            this,
            "Очистка оповещений",
            "Список оповещений пуст."
            );
        return;
    }

    int triggeredCount = 0;

    for (const PriceAlertUiData &alert : priceAlerts)
    {
        if (!alert.active)
            ++triggeredCount;
    }

    if (triggeredCount == 0)
    {
        QMessageBox::information(
            this,
            "Очистка оповещений",
            "Сработавших оповещений нет."
            );
        return;
    }

    QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        "Очистка оповещений",
        "Удалить все сработавшие оповещения?\n\n"
        "Будет удалено: " + QString::number(triggeredCount),
        QMessageBox::Yes | QMessageBox::No
        );

    if (answer != QMessageBox::Yes)
        return;

    for (int i = priceAlerts.size() - 1; i >= 0; --i)
    {
        if (!priceAlerts[i].active)
            priceAlerts.removeAt(i);
    }

    refreshAlertsList();
    saveAlertsToFile();
}

QString ai_project::alertsFilePath() const
{
    return QCoreApplication::applicationDirPath() + "/alerts.json";
}

void ai_project::saveAlertsToFile() const
{
    QJsonArray alertsArray;

    for (const PriceAlertUiData &alert : priceAlerts)
    {
        QJsonObject object;

        object["coinId"] = alert.coinId;
        object["coinSymbol"] = alert.coinSymbol;
        object["targetPrice"] = alert.targetPrice;
        object["aboveCondition"] = alert.aboveCondition;
        object["active"] = alert.active;

        alertsArray.append(object);
    }

    QJsonDocument document(alertsArray);

    QFile file(alertsFilePath());

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(document.toJson(QJsonDocument::Indented));
    file.close();
}

void ai_project::loadAlertsFromFile()
{
    QFile file(alertsFilePath());

    if (!file.exists())
        return;

    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return;

    QJsonArray alertsArray = document.array();

    priceAlerts.clear();

    for (const QJsonValue &value : alertsArray)
    {
        if (!value.isObject())
            continue;

        QJsonObject object = value.toObject();

        PriceAlertUiData alert;
        alert.coinId = object.value("coinId").toString();
        alert.coinSymbol = object.value("coinSymbol").toString();
        alert.targetPrice = object.value("targetPrice").toDouble();
        alert.aboveCondition = object.value("aboveCondition").toBool();
        alert.active = object.value("active").toBool();

        if (alert.coinId.isEmpty() || alert.coinSymbol.isEmpty() || alert.targetPrice <= 0.0)
            continue;

        priceAlerts.append(alert);
    }
}

QString ai_project::settingsFilePath() const
{
    return QCoreApplication::applicationDirPath() + "/settings.json";
}

void ai_project::saveSettingsToFile() const
{
    QJsonObject object;

    object["selectedCoinId"] = selectedCoinId;
    object["selectedPeriod"] = selectedPeriod;
    object["candleMode"] = candleMode;
    object["showLevels"] = showLevels;
    object["autoUpdate"] = autoUpdateButton && autoUpdateButton->isChecked();

    if (fromCoinCombo)
        object["converterFrom"] = fromCoinCombo->currentData().toString();

    if (toCoinCombo)
        object["converterTo"] = toCoinCombo->currentData().toString();

    QJsonDocument document(object);

    QFile file(settingsFilePath());

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(document.toJson(QJsonDocument::Indented));
    file.close();
}

void ai_project::loadSettingsFromFile()
{
    QFile file(settingsFilePath());

    if (!file.exists())
        return;

    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;

    QJsonObject object = document.object();

    QString loadedCoinId = object.value("selectedCoinId").toString();

    if (findCoinIndexById(loadedCoinId) >= 0)
        selectedCoinId = loadedCoinId;

    QString loadedPeriod = object.value("selectedPeriod").toString();

    if (loadedPeriod == "24ч" || loadedPeriod == "7д" ||
        loadedPeriod == "30д" || loadedPeriod == "90д")
    {
        selectedPeriod = loadedPeriod;
    }

    candleMode = object.value("candleMode").toBool(false);
    showLevels = object.value("showLevels").toBool(true);

    lineButton->setChecked(!candleMode);
    candleButton->setChecked(candleMode);
    levelsButton->setChecked(showLevels);

    autoUpdateButton->setChecked(object.value("autoUpdate").toBool(false));
    autoUpdateButton->setText(autoUpdateButton->isChecked()
                                  ? "Автообновление: вкл"
                                  : "Автообновление: выкл");

    applyPeriodButtonState();

    QString converterFrom = object.value("converterFrom").toString();
    QString converterTo = object.value("converterTo").toString();

    int fromIndex = fromCoinCombo->findData(converterFrom);
    int toIndex = toCoinCombo->findData(converterTo);

    if (fromIndex >= 0)
        fromCoinCombo->setCurrentIndex(fromIndex);

    if (toIndex >= 0)
        toCoinCombo->setCurrentIndex(toIndex);
}

void ai_project::applyPeriodButtonState()
{
    period24Button->setChecked(selectedPeriod == "24ч");
    period7Button->setChecked(selectedPeriod == "7д");
    period30Button->setChecked(selectedPeriod == "30д");
    period90Button->setChecked(selectedPeriod == "90д");

    if (selectedPeriod == "24ч")
        periodInfoLabel->setText("Период: 24 часа, шаг: 1 час");
    else if (selectedPeriod == "7д")
        periodInfoLabel->setText("Период: 7 дней, шаг: 4 часа");
    else if (selectedPeriod == "30д")
        periodInfoLabel->setText("Период: 30 дней, шаг: 1 день");
    else
        periodInfoLabel->setText("Период: 90 дней, шаг: 1 день");
}
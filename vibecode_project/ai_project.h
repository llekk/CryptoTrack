#ifndef AI_PROJECT_H
#define AI_PROJECT_H

#include <QMainWindow>
#include <QVector>
#include <QString>

class QLabel;
class QListWidget;
class QPushButton;
class QComboBox;
class QLineEdit;
class QFrame;
class MiniChartWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;
class QTimer;

struct CandleUiData
{
    double open;
    double high;
    double low;
    double close;
};

struct CoinUiData
{
    QString id;
    QString symbol;
    QString name;

    double price;
    double changePercent;
    double volume;

    QVector<double> history;
    QVector<double> volumes;
    QVector<double> rsi;
    QVector<CandleUiData> candles;
};

struct PriceAlertUiData
{
    QString coinId;
    QString coinSymbol;

    double targetPrice;
    bool aboveCondition;
    bool active;
};

class ai_project : public QMainWindow
{
    Q_OBJECT

public:
    explicit ai_project(QWidget *parent = nullptr);
    ~ai_project();

private:
    QVector<CoinUiData> coins;

    QVector<PriceAlertUiData> priceAlerts;

    QNetworkAccessManager *networkManager;

    QTimer *autoUpdateTimer;

    QListWidget *coinList;

    QLabel *coinNameLabel;
    QLabel *coinPriceLabel;
    QLabel *coinChangeLabel;
    QLabel *periodInfoLabel;
    QLabel *lastUpdateLabel;

    MiniChartWidget *priceChart;
    MiniChartWidget *volumeChart;
    MiniChartWidget *rsiChart;

    QPushButton *period24Button;
    QPushButton *period7Button;
    QPushButton *period30Button;
    QPushButton *period90Button;

    QPushButton *lineButton;
    QPushButton *candleButton;
    QPushButton *levelsButton;
    QPushButton *refreshButton;
    QPushButton *autoUpdateButton;

    QComboBox *alertCoinCombo;
    QComboBox *alertConditionCombo;
    QLineEdit *alertPriceEdit;
    QListWidget *alertsList;

    QComboBox *fromCoinCombo;
    QComboBox *toCoinCombo;
    QLineEdit *amountEdit;
    QLabel *converterResultLabel;

    QString selectedCoinId;
    QString selectedPeriod;
    bool candleMode;
    bool showLevels;

    bool marketRequestInProgress;
    qint64 lastMarketRequestMs;

private:
    void setupInterface();
    void setupStyle();
    void fillDemoData();
    void buildCoinList();

    QFrame *createCard();
    QPushButton *createSmallButton(const QString &text);

    void selectCoin(int row);
    void updateCenterPanel(const CoinUiData &coin);
    void updateConverter();
    void addAlert();
    void deleteSelectedAlert();
    void clearTriggeredAlerts();
    void refreshAlertsList();
    void checkPriceAlerts();
    QString alertText(const PriceAlertUiData &alert) const;

    void saveAlertsToFile() const;
    void loadAlertsFromFile();
    QString alertsFilePath() const;

    void saveSettingsToFile() const;
    void loadSettingsFromFile();
    QString settingsFilePath() const;
    void applyPeriodButtonState();

    void toggleAutoUpdate();
    void refreshDemoData();

    void fetchMarketData();
    void handleMarketDataReply(QNetworkReply *reply);
    void updateCoinFromJson(const QJsonObject &object);

    void fetchHistoryData(const QString &coinId, const QString &period);
    void handleHistoryDataReply(QNetworkReply *reply, const QString &coinId, const QString &period);

    int daysForPeriod(const QString &period) const;
    int targetPointCountForPeriod(const QString &period) const;

    QVector<double> reduceToClosingValues(const QVector<double> &source, int targetCount) const;
    QVector<double> reduceToAverageValues(const QVector<double> &source, int targetCount) const;
    QVector<double> calculateRsiValues(const QVector<double> &prices) const;

    QVector<CandleUiData> buildCandlesFromPrices(const QVector<double> &source, int targetCount) const;
    QVector<double> closeValuesFromCandles(const QVector<CandleUiData> &candles) const;

    QVector<double> calculateSupportLevels(const QVector<double> &prices) const;
    QVector<double> calculateResistanceLevels(const QVector<double> &prices) const;

    int findCoinIndexById(const QString &id) const;
    QString formatPrice(double value) const;
    QString formatVolume(double value) const;
};

#endif
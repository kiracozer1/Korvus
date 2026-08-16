#pragma once
#include <QTableWidget>
#include <QMainWindow>
#include <QDateTime>
#include <QProcess>
#include <QLineEdit>


#include "firewallmanager.h"
#include "auditmanager.h"

class QStackedWidget;
class QLabel;
class QComboBox;
class QListWidget;
class QProgressBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onMonitorOutputReady();
    void onRefreshClicked();
    void onAddRuleClicked();
    void onDeleteRuleClicked();
    void onEnableRuleClicked();
    void onEditRuleClicked();
    void onDisableRuleClicked();
    void onApplyPolicyClicked();
 

private:
    QWidget *buildDashboardPage();
    QWidget *buildRulesPage();
     QWidget *buildMonitorPage();
    QWidget *buildLogPage();
   QWidget *buildBlockedIpsPage();
    void applyMonitorFilter();
    void applyTheme(bool dark);
    void updateTrafficPieChart();
    void detectPortScans();
    void refreshBlockedIpsList();
    void onUnblockIpClicked();
    void onManualBlockIpClicked();

    void refreshDashboard();
    void refreshRulesTable();
    void refreshLogPage();
    void appendMonitorLine(const QString &line);
    QLabel *m_trafficPieChart = nullptr;
QLabel *m_pieLegendLabel = nullptr;
QLabel *m_dashboardLogo = nullptr;
int m_allowedPacketCount = 0;
int m_blockedPacketCount = 0;

    FirewallManager m_firewallManager;
    AuditManager m_auditManager;
    QDateTime m_lastRefresh;

    QStackedWidget *m_pages = nullptr;
    QLineEdit *m_manualIpInput = nullptr;

   
    QLabel *m_statusValue = nullptr;
    QLabel *m_policyValue = nullptr;
    QLabel *m_ruleCountValue = nullptr;
    QLabel *m_lastRefreshValue = nullptr;

  
    QTableWidget *m_rulesTable = nullptr;
    QComboBox *m_policyCombo = nullptr;
    QComboBox *m_themeCombo = nullptr;

  
    QListWidget *m_logList = nullptr;


    QTableWidget *m_monitorTable = nullptr;

    QComboBox *m_protocolFilter = nullptr;
    QComboBox *m_directionFilter = nullptr;
    QComboBox *m_typeFilter = nullptr;

    QLineEdit *m_searchFilter = nullptr;

    QProcess *m_monitorProcess = nullptr;

    QLabel *m_portScanAlertLabel = nullptr;

    QProgressBar *m_allowedBar = nullptr;
    QProgressBar *m_blockedBar = nullptr;

    QListWidget *m_blockedIpsList = nullptr;
};
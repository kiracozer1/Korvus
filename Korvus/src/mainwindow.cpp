#include "mainwindow.h"
#include <QFont>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QComboBox>
#include <QLineEdit>
#include <QApplication>
#include <QSpinBox>
#include <QPushButton>
#include <QToolBar>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QStyleFactory>
#include <QPainter>
#include <QGridLayout>
#include <QFrame>
#include <QProgressBar>
#include <QHash>
#include <QSet>


namespace {
constexpr int kMinWindowWidth = 900;
constexpr int kMinWindowHeight = 600;
constexpr int kMenuWidth = 160;
constexpr int kMaxPort = 65535;
constexpr int kMaxMonitorLines = 300;
}
namespace {
QFrame *createStatCard(const QString &icon, const QString &title, QLabel **valueLabelOut, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("statCard"));

    auto *layout = new QVBoxLayout(card);

    auto *titleLabel = new QLabel(icon + QStringLiteral("  ") + title, card);
    titleLabel->setObjectName(QStringLiteral("statTitle"));

    auto *valueLabel = new QLabel(QStringLiteral("—"), card);
    valueLabel->setObjectName(QStringLiteral("statValue"));

    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);

    *valueLabelOut = valueLabel;
    return card;
}
}
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Korvus"));
    resize(kMinWindowWidth, kMinWindowHeight);

    auto *toolBar = addToolBar(QStringLiteral("Main"));
    toolBar->setMovable(false);
    QAction *refreshAction = toolBar->addAction(QStringLiteral("Refresh"));
    connect(refreshAction, &QAction::triggered, this, &MainWindow::onRefreshClicked);
    toolBar->addSeparator();

m_themeCombo = new QComboBox(this);
m_themeCombo->addItems({"Light","Dark"});
toolBar->addWidget(m_themeCombo);

connect(m_themeCombo,
        &QComboBox::currentTextChanged,
        this,
        [this](const QString &text)
{
    const bool dark = (text == "Dark");
    applyTheme(dark);
    updateTrafficPieChart();

    if (m_dashboardLogo) {
        const QString path = dark ? QStringLiteral(":/logo_dark.png") : QStringLiteral(":/logo_light.png");
        m_dashboardLogo->setPixmap(QPixmap(path).scaledToWidth(220, Qt::SmoothTransformation));
    }
});
applyTheme(false);

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);

    auto *menu = new QListWidget(central);
menu->addItem(QStringLiteral("Dashboard"));
    menu->addItem(QStringLiteral("Rules"));
    menu->addItem(QStringLiteral("Monitor"));
    menu->addItem(QStringLiteral("Blocked IPs"));
    menu->addItem(QStringLiteral("Log"));

    m_pages = new QStackedWidget(central);
    m_pages->addWidget(buildDashboardPage());
    m_pages->addWidget(buildRulesPage());
    m_pages->addWidget(buildMonitorPage());
    m_pages->addWidget(buildBlockedIpsPage());
    m_pages->addWidget(buildLogPage());

    connect(menu, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    menu->setCurrentRow(0);

    layout->addWidget(menu);
    layout->addWidget(m_pages, 1);
    setCentralWidget(central);

    onRefreshClicked();

    m_monitorProcess = new QProcess(this);
       connect(m_monitorProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onMonitorOutputReady);
    m_monitorProcess->start(QStringLiteral("journalctl"), {"-k", "-f", "-n", "0", "--no-pager", "-g", "Korvus"});
}
    



MainWindow::~MainWindow()
{
    if (m_monitorProcess && m_monitorProcess->state() != QProcess::NotRunning) {
        m_monitorProcess->kill();
        m_monitorProcess->waitForFinished(1000);
    }
}

QWidget *MainWindow::buildDashboardPage()
{
      auto *page = new QWidget;
    auto *outerLayout = new QVBoxLayout(page);
    m_dashboardLogo = new QLabel(page);
    m_dashboardLogo->setPixmap(QPixmap(QStringLiteral(":/logo_light.png")).scaledToWidth(220, Qt::SmoothTransformation));
    m_dashboardLogo->setAlignment(Qt::AlignHCenter);
    outerLayout->addWidget(m_dashboardLogo);
    outerLayout->setSpacing(20);
    outerLayout->setContentsMargins(20, 20, 20, 20);

    auto *chartCard = new QFrame(page);
    chartCard->setObjectName(QStringLiteral("statCard"));
    chartCard->setMaximumWidth(320);
    auto *chartLayout = new QVBoxLayout(chartCard);

    auto *chartTitle = new QLabel(QStringLiteral("📊  Allowed vs Blocked Traffic"), chartCard);
    chartTitle->setObjectName(QStringLiteral("statTitle"));
    chartTitle->setAlignment(Qt::AlignHCenter);
    chartLayout->addWidget(chartTitle);

    m_trafficPieChart = new QLabel(chartCard);
    m_trafficPieChart->setFixedSize(160, 160);
    m_trafficPieChart->setAlignment(Qt::AlignCenter);
    chartLayout->addWidget(m_trafficPieChart, 0, Qt::AlignHCenter);

   m_allowedBar = new QProgressBar(chartCard);
    m_allowedBar->setRange(0, 100);
    m_allowedBar->setFormat(QStringLiteral("Allow %p%"));
    m_allowedBar->setStyleSheet(QStringLiteral(
        "QProgressBar::chunk { background-color: #22c55e; } QProgressBar { text-align: center; }"));
    chartLayout->addWidget(m_allowedBar);
   
    m_blockedBar = new QProgressBar(chartCard);
    m_blockedBar->setRange(0, 100);
    m_blockedBar->setFormat(QStringLiteral("Block %p%"));
    m_blockedBar->setStyleSheet(QStringLiteral(
        "QProgressBar::chunk { background-color: #ef4444; } QProgressBar { text-align: center; }"));
    chartLayout->addWidget(m_blockedBar);
    m_portScanAlertLabel = new QLabel(chartCard);
    m_portScanAlertLabel->setAlignment(Qt::AlignHCenter);
    m_portScanAlertLabel->setWordWrap(true);
    m_portScanAlertLabel->setStyleSheet(QStringLiteral("color: #f59e0b; font-weight: 600;"));
    chartLayout->addWidget(m_portScanAlertLabel);

    auto *chartRow = new QHBoxLayout;
    chartRow->addStretch();
    chartRow->addWidget(chartCard);
    chartRow->addStretch();

    auto *cardsGrid = new QGridLayout;
    cardsGrid->setSpacing(16);

    cardsGrid->addWidget(createStatCard(QStringLiteral("🛡"), QStringLiteral("Firewall Status"), &m_statusValue, page), 0, 0);
    cardsGrid->addWidget(createStatCard(QStringLiteral("📐"), QStringLiteral("Default Policy"), &m_policyValue, page), 0, 1);
    cardsGrid->addWidget(createStatCard(QStringLiteral("📋"), QStringLiteral("Rule Count"), &m_ruleCountValue, page), 1, 0);
    cardsGrid->addWidget(createStatCard(QStringLiteral("🕓"), QStringLiteral("Last Refresh"), &m_lastRefreshValue, page), 1, 1);

    outerLayout->addLayout(chartRow);
    outerLayout->addLayout(cardsGrid);
    outerLayout->addStretch();

    updateTrafficPieChart();

    return page;
}

QWidget *MainWindow::buildRulesPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    m_rulesTable = new QTableWidget(0, 6, page);
    m_rulesTable->setHorizontalHeaderLabels(
        {QStringLiteral("Rule Name"), QStringLiteral("Direction"), QStringLiteral("Protocol"),
         QStringLiteral("Port"), QStringLiteral("Action"), QStringLiteral("Enabled")});
    m_rulesTable->horizontalHeader()->setStretchLastSection(true);
    m_rulesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rulesTable->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *buttonRow = new QHBoxLayout;
    auto *addButton = new QPushButton(QStringLiteral("Add Rule"), page);
    auto *editButton = new QPushButton(QStringLiteral("Edit Rule"), page);
    auto *deleteButton = new QPushButton(QStringLiteral("Delete Rule"), page);
    auto *enableButton = new QPushButton(QStringLiteral("Enable"), page);
    auto *disableButton = new QPushButton(QStringLiteral("Disable"), page);
    
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(editButton);
    buttonRow->addWidget(deleteButton);
    buttonRow->addWidget(enableButton);
    buttonRow->addWidget(disableButton);
    buttonRow->addStretch();

    auto *policyRow = new QHBoxLayout;
    auto *policyLabel = new QLabel(QStringLiteral("Default Policy:"), page);
    m_policyCombo = new QComboBox(page);
    m_policyCombo->addItems({QStringLiteral("ALLOW"), QStringLiteral("DENY")});
    auto *applyPolicyButton = new QPushButton(QStringLiteral("Apply Policy"), page);
    policyRow->addWidget(policyLabel);
    policyRow->addWidget(m_policyCombo);
    policyRow->addWidget(applyPolicyButton);
    policyRow->addStretch();

    layout->addWidget(m_rulesTable);
    layout->addLayout(buttonRow);
    layout->addLayout(policyRow);


    connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddRuleClicked);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::onEditRuleClicked);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::onDeleteRuleClicked);
    connect(enableButton, &QPushButton::clicked, this, &MainWindow::onEnableRuleClicked);
    connect(disableButton, &QPushButton::clicked, this, &MainWindow::onDisableRuleClicked);
    connect(applyPolicyButton, &QPushButton::clicked, this, &MainWindow::onApplyPolicyClicked);

    return page;
}

QWidget *MainWindow::buildLogPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *infoLabel = new QLabel(QStringLiteral("Activity Logs"), page);
    infoLabel->setWordWrap(true);

    m_logList = new QListWidget(page);
    layout->addWidget(infoLabel);
    layout->addWidget(m_logList);

    return page;
}
QWidget *MainWindow::buildBlockedIpsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *infoLabel = new QLabel(
        QStringLiteral("Blocked IP addresses"), page);
    infoLabel->setWordWrap(true);

    auto *inputRow = new QHBoxLayout;
    m_manualIpInput = new QLineEdit(page);
    m_manualIpInput->setPlaceholderText(QStringLiteral("You can block an IP or valid IP range."));
    auto *blockButton = new QPushButton(QStringLiteral("Block IP"), page);
    inputRow->addWidget(m_manualIpInput);
    inputRow->addWidget(blockButton);

    m_blockedIpsList = new QListWidget(page);

    auto *unblockButton = new QPushButton(QStringLiteral("Unblock Selected"), page);
    connect(unblockButton, &QPushButton::clicked, this, &MainWindow::onUnblockIpClicked);
    connect(blockButton, &QPushButton::clicked, this, &MainWindow::onManualBlockIpClicked);

    layout->addWidget(infoLabel);
    layout->addLayout(inputRow);
    layout->addWidget(m_blockedIpsList);
    layout->addWidget(unblockButton);

    return page;
}
QWidget *MainWindow::buildMonitorPage()

{
    
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

  
    auto *toolRow = new QHBoxLayout;

    toolRow->addWidget(new QLabel(QStringLiteral("Protocol:"), page));
    m_protocolFilter = new QComboBox(page);
    m_protocolFilter->addItems({
        QStringLiteral("All"),
        QStringLiteral("TCP"),
        QStringLiteral("UDP"),
        QStringLiteral("ICMP")
    });
    toolRow->addWidget(m_protocolFilter);

    toolRow->addSpacing(10);

    toolRow->addWidget(new QLabel(QStringLiteral("Direction:"), page));
    m_directionFilter = new QComboBox(page);
    m_directionFilter->addItems({
        QStringLiteral("All"),
        QStringLiteral("In"),
        QStringLiteral("Out")
    });
    toolRow->addWidget(m_directionFilter);

    toolRow->addSpacing(10);

    toolRow->addWidget(new QLabel(QStringLiteral("Type:"), page));
    m_typeFilter = new QComboBox(page);
    m_typeFilter->addItems({
        QStringLiteral("All"),
    QStringLiteral("Traffic"),
    QStringLiteral("Allow"),
    QStringLiteral("Block")
    });
    toolRow->addWidget(m_typeFilter);

    toolRow->addSpacing(10);

    toolRow->addWidget(new QLabel(QStringLiteral("Search:"), page));
    m_searchFilter = new QLineEdit(page);
    m_searchFilter->setPlaceholderText(QStringLiteral("IP / Port / Protocol"));
    m_searchFilter->setClearButtonEnabled(true);
    toolRow->addWidget(m_searchFilter);

    toolRow->addSpacing(10);

    auto *clearButton = new QPushButton(QStringLiteral("Clear"), page);
    toolRow->addWidget(clearButton);

    toolRow->addStretch();

    
    m_monitorTable = new QTableWidget(0, 6, page);

    m_monitorTable->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Type"),
        QStringLiteral("Direction"),
        QStringLiteral("Protocol"),
        QStringLiteral("Source"),
        QStringLiteral("Destination")
    });

    m_monitorTable->horizontalHeader()->setStretchLastSection(true);
    m_monitorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_monitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_monitorTable->setSelectionMode(QAbstractItemView::SingleSelection);

    layout->addLayout(toolRow);
    layout->addWidget(m_monitorTable);

    
    connect(clearButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                m_monitorTable->setRowCount(0);
            });

    connect(m_protocolFilter,
            &QComboBox::currentTextChanged,
            this,
            &MainWindow::applyMonitorFilter);

    connect(m_directionFilter,
            &QComboBox::currentTextChanged,
            this,
            &MainWindow::applyMonitorFilter);

    connect(m_typeFilter,
            &QComboBox::currentTextChanged,
            this,
            &MainWindow::applyMonitorFilter);

    connect(m_searchFilter,
            &QLineEdit::textChanged,
            this,
            &MainWindow::applyMonitorFilter);

    return page;
}
namespace {
QString extractLogField(const QString &line, const QString &key)
{
    const QString marker = key + QStringLiteral("=");
    const int start = line.indexOf(marker);
    if (start < 0)
        return {};

    const int valueStart = start + marker.size();
    int valueEnd = line.indexOf(QChar(' '), valueStart);
    if (valueEnd < 0)
        valueEnd = line.size();
    return line.mid(valueStart, valueEnd - valueStart);
}
}


void MainWindow::appendMonitorLine(const QString &line)
{
   
    if (!line.contains(QStringLiteral("Korvus-")))
        return;
QString type;
    if (line.contains(QStringLiteral("Korvus-TRAFFIC-")))
        type = QStringLiteral("Traffic");
    else if (line.contains(QStringLiteral("Korvus-ALLOW-")))
        type = QStringLiteral("Allow");
    else if (line.contains(QStringLiteral("Korvus-BLOCK-")))
        type = QStringLiteral("Block");
    else
        return;

    const QString direction = line.contains(QStringLiteral("-IN:")) ? QStringLiteral("In") : QStringLiteral("Out");

    if (type == QStringLiteral("Allow"))
        ++m_allowedPacketCount;
    else if (type == QStringLiteral("Block"))
        ++m_blockedPacketCount;

    if (type != QStringLiteral("Traffic"))
        updateTrafficPieChart();
    const QStringList tokens = line.split(QChar(' '), Qt::SkipEmptyParts);
    const QString time = tokens.size() >= 3 ? tokens[0] + QStringLiteral(" ") + tokens[1] + QStringLiteral(" ") + tokens[2]
                                             : QString();

    const QString proto = extractLogField(line, QStringLiteral("PROTO"));
    const QString src = extractLogField(line, QStringLiteral("SRC"));
    const QString dst = extractLogField(line, QStringLiteral("DST"));
    const QString spt = extractLogField(line, QStringLiteral("SPT"));
    const QString dpt = extractLogField(line, QStringLiteral("DPT"));

    const QString sourceText = spt.isEmpty() ? src : src + QStringLiteral(":") + spt;
    const QString destText = dpt.isEmpty() ? dst : dst + QStringLiteral(":") + dpt;

    m_monitorTable->insertRow(0);
    m_monitorTable->setItem(0, 0, new QTableWidgetItem(time));
    m_monitorTable->setItem(0, 1, new QTableWidgetItem(type));
    m_monitorTable->setItem(0, 2, new QTableWidgetItem(direction));
    m_monitorTable->setItem(0, 3, new QTableWidgetItem(proto));
    m_monitorTable->setItem(0, 4, new QTableWidgetItem(sourceText));
    m_monitorTable->setItem(0, 5, new QTableWidgetItem(destText));
  if (type == QStringLiteral("Block")) {
        for (int col = 0; col < m_monitorTable->columnCount(); ++col)
            m_monitorTable->item(0, col)->setForeground(Qt::red);
    } else if (type == QStringLiteral("Allow")) {
        for (int col = 0; col < m_monitorTable->columnCount(); ++col)
            m_monitorTable->item(0, col)->setForeground(QColor("#2ecc71"));
    }
    while (m_monitorTable->rowCount() > kMaxMonitorLines)
        m_monitorTable->removeRow(m_monitorTable->rowCount() - 1);
        
        
        applyMonitorFilter();
        detectPortScans();

}

void MainWindow::onMonitorOutputReady()
{
    const QString output = QString::fromLocal8Bit(m_monitorProcess->readAllStandardOutput());
    const QStringList lines = output.split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
        appendMonitorLine(line);
}

void MainWindow::refreshDashboard()
{
    m_statusValue->setText(m_firewallManager.isActive() ? QStringLiteral("Active") : QStringLiteral("Inactive"));
    m_policyValue->setText(m_firewallManager.defaultPolicy());

    int enabledCount = 0;
    for (const Rule &rule : m_firewallManager.rules()) {
        if (rule.enabled)
            ++enabledCount;
    }
    m_ruleCountValue->setText(QString::number(enabledCount));
    m_lastRefreshValue->setText(m_lastRefresh.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    updateTrafficPieChart();
}

void MainWindow::refreshRulesTable()
{
  const QVector<Rule> &rules = m_firewallManager.rules();
    m_rulesTable->setRowCount(rules.size());

    for (int row = 0; row < rules.size(); ++row) {
        const Rule &rule = rules.at(row);
        m_rulesTable->setItem(row, 0, new QTableWidgetItem(rule.name)); 
        m_rulesTable->setItem(row, 1, new QTableWidgetItem(directionToString(rule.direction)));
        m_rulesTable->setItem(row, 2, new QTableWidgetItem(protocolToString(rule.protocol)));
        m_rulesTable->setItem(row, 3, new QTableWidgetItem(QString::number(rule.port)));
        m_rulesTable->setItem(row, 4, new QTableWidgetItem(actionToString(rule.action)));
        m_rulesTable->setItem(row, 5, new QTableWidgetItem(rule.enabled ? QStringLiteral("Yes") : QStringLiteral("No")));
    }

    m_policyCombo->setCurrentText(m_firewallManager.defaultPolicy());
}

void MainWindow::refreshLogPage()
{
    if (!m_logList)
        return;

    m_logList->clear();
    m_logList->addItems(m_auditManager.recentActions());
}

void MainWindow::onRefreshClicked()
{
    m_firewallManager.refresh();
    m_lastRefresh = QDateTime::currentDateTime();

    refreshDashboard();
    refreshRulesTable();
    refreshLogPage();
    refreshBlockedIpsList();
}

void MainWindow::onAddRuleClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Add Rule"));

    auto *form = new QFormLayout(&dialog);

    
    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("For ex:ssh block..."));

    auto *directionCombo = new QComboBox(&dialog);
    directionCombo->addItems({QStringLiteral("Inbound"), QStringLiteral("Outbound")});

    auto *protocolCombo = new QComboBox(&dialog);
    protocolCombo->addItems({QStringLiteral("TCP"), QStringLiteral("UDP")});

    auto *portSpin = new QSpinBox(&dialog);
    portSpin->setRange(1, kMaxPort);

    auto *actionCombo = new QComboBox(&dialog);
    actionCombo->addItems({QStringLiteral("Accept"), QStringLiteral("Drop")});

    form->addRow(QStringLiteral("Rule Name:"), nameEdit); 
    form->addRow(QStringLiteral("Direction:"), directionCombo);
    form->addRow(QStringLiteral("Protocol:"), protocolCombo);
    form->addRow(QStringLiteral("Port:"), portSpin);
    form->addRow(QStringLiteral("Action:"), actionCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    Rule rule;
    rule.name = nameEdit->text().trimmed(); 
    rule.direction = directionCombo->currentIndex() == 0 ? Direction::Inbound : Direction::Outbound;
    rule.protocol = protocolCombo->currentIndex() == 0 ? Protocol::Tcp : Protocol::Udp;
    rule.port = portSpin->value();
    rule.action = actionCombo->currentIndex() == 0 ? Action::Accept : Action::Drop;
    rule.enabled = true;

    m_firewallManager.addRule(rule);
    onRefreshClicked();
}

void MainWindow::onDeleteRuleClicked()
{
    const int row = m_rulesTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("Delete Rule"), QStringLiteral("Select a rule first."));
        return;
    }

    m_firewallManager.removeRule(row);
    onRefreshClicked();
}

void MainWindow::onEnableRuleClicked()
{
    const int row = m_rulesTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("Enable Rule"), QStringLiteral("Select a rule first."));
        return;
    }

    m_firewallManager.setRuleEnabled(row, true);
    onRefreshClicked();
}

void MainWindow::onDisableRuleClicked()
{
    const int row = m_rulesTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("Disable Rule"), QStringLiteral("Select a rule first."));
        return;
    }

    m_firewallManager.setRuleEnabled(row, false);
    onRefreshClicked();
}

void MainWindow::onApplyPolicyClicked()
{
    m_firewallManager.setDefaultPolicy(m_policyCombo->currentText());
    onRefreshClicked();
}
void MainWindow::applyMonitorFilter()
{
    const QString protocol =
        m_protocolFilter->currentText();

    const QString direction =
        m_directionFilter->currentText();

    const QString type =
        m_typeFilter->currentText();

    const QString search =
        m_searchFilter->text().trimmed();

    for (int row = 0; row < m_monitorTable->rowCount(); ++row)
    {
        bool visible = true;

        QString rowType =
            m_monitorTable->item(row,1)->text();

        QString rowDirection =
            m_monitorTable->item(row,2)->text();

        QString rowProtocol =
            m_monitorTable->item(row,3)->text();

        QString rowSource =
            m_monitorTable->item(row,4)->text();

        QString rowDestination =
            m_monitorTable->item(row,5)->text();

        
        if (protocol != "All" &&
            rowProtocol.compare(protocol, Qt::CaseInsensitive) != 0)
        {
            visible = false;
        }

       
        if (visible &&
            direction != "All" &&
            rowDirection.compare(direction, Qt::CaseInsensitive) != 0)
        {
            visible = false;
        }

        
        if (visible &&
            type != "All" &&
            rowType.compare(type, Qt::CaseInsensitive) != 0)
        {
            visible = false;
        }

       
        if (visible && !search.isEmpty())
        {
            QString allText =
                    rowSource + " " +
                    rowDestination + " " +
                    rowProtocol + " " +
                    rowDirection + " " +
                    rowType;

            if (!allText.contains(search, Qt::CaseInsensitive))
                visible = false;
        }

        m_monitorTable->setRowHidden(row, !visible);
    }
}
void MainWindow::applyTheme(bool dark)
{
    if (dark)
    {
        qApp->setStyleSheet(
          
            "QWidget { background-color: #0b0f19; color: #cbd5e1; font-family: 'Segoe UI', Arial, sans-serif; font-size: 13px; }"
            "QMainWindow { background-color: #0b0f19; }"
            
         
            "QListWidget { background-color: #111827; border: 1px solid #1f2937; outline: none; padding: 4px; }"
            "QListWidget::item { padding: 10px 14px; margin-bottom: 2px; border-radius: 0px; color: #9ca3af; }"
            "QListWidget::item:selected { background-color: #1d4ed8; color: #ffffff; font-weight: 600; border-left: 3px solid #60a5fa; }"
            "QListWidget::item:hover:not(:selected) { background-color: #1f2937; color: #e5e7eb; }"

           
            "QTableWidget { background-color: #0b0f19; border: 1px solid #1f2937; gridline-color: #1f2937; selection-background-color: #1d4ed8; selection-color: #ffffff; }"
            "QTableWidget::item { padding: 8px; }"
            "QHeaderView::section { background-color: #111827; color: #93c5fd; padding: 8px; border: none; border-bottom: 2px solid #1d4ed8; font-weight: 600; }"
            
         
            "QPushButton { background-color: #1f2937; color: #e5e7eb; border: 1px solid #374151; padding: 7px 16px; }"
            "QPushButton:hover { background-color: #2563eb; border: 1px solid #60a5fa; color: #ffffff; }"
            "QPushButton:pressed { background-color: #1d4ed8; }"
           
            "QLineEdit, QComboBox, QSpinBox { background-color: #111827; border: 1px solid #374151; padding: 5px 8px; color: #ffffff; selection-background-color: #2563eb; }"
            "QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid #3b82f6; }"
            "QComboBox::drop-down { border: none; width: 22px; background: #1f2937; }"
            
            "QToolBar { background-color: #111827; border-bottom: 1px solid #1f2937; spacing: 8px; padding: 6px; }"
            
         
            "QLabel { color: #cbd5e1; }"
            "QDialog { background-color: #111827; border: 1px solid #374151; }"
            "QFrame#statCard { background-color: #111827; border: 1px solid #1f2937; border-radius: 10px; padding: 14px; }"
            "QLabel#statTitle { color: #93c5fd; font-size: 12px; font-weight: 600; }"
            "QLabel#statValue { color: #ffffff; font-size: 22px; font-weight: 700; padding-top: 4px; }"
        );
    }
    else
    {
        qApp->setStyleSheet(
            
            "QWidget { background-color: #f1f5f9; color: #1e293b; font-family: 'Segoe UI', Arial, sans-serif; font-size: 13px; }"
            "QMainWindow { background-color: #f1f5f9; }"
            
        
            "QListWidget { background-color: #ffffff; border: 1px solid #cbd5e1; outline: none; padding: 4px; }"
            "QListWidget::item { padding: 10px 14px; margin-bottom: 2px; border-radius: 0px; color: #475569; }"
            "QListWidget::item:selected { background-color: #dbeafe; color: #1d4ed8; font-weight: 600; border-left: 3px solid #2563eb; }"
            "QListWidget::item:hover:not(:selected) { background-color: #f8fafc; color: #0f172a; }"

         
            "QTableWidget { background-color: #ffffff; border: 1px solid #cbd5e1; gridline-color: #e2e8f0; selection-background-color: #dbeafe; selection-color: #1e40af; }"
            "QTableWidget::item { padding: 8px; }"
            "QHeaderView::section { background-color: #e2e8f0; color: #1e293b; padding: 8px; border: none; border-bottom: 2px solid #cbd5e1; font-weight: 600; }"
           
            "QPushButton { background-color: #ffffff; color: #1e293b; border: 1px solid #cbd5e1; padding: 7px 16px; }"
            "QPushButton:hover { background-color: #f8fafc; border: 1px solid #2563eb; color: #2563eb; }"
            "QPushButton:pressed { background-color: #2563eb; color: #ffffff; }"
            
           
            "QLineEdit, QComboBox, QSpinBox { background-color: #ffffff; border: 1px solid #cbd5e1; padding: 5px 8px; color: #0f172a; selection-background-color: #2563eb; selection-color: #ffffff; }"
            "QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid #2563eb; }"
            "QComboBox::drop-down { border: none; width: 22px; background: #e2e8f0; }"
            
           
            "QToolBar { background-color: #ffffff; border-bottom: 1px solid #cbd5e1; spacing: 8px; padding: 6px; }"
            
            
            "QLabel { color: #1e293b; }"
            "QDialog { background-color: #ffffff; border: 1px solid #cbd5e1; }"

            "QFrame#statCard { background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 10px; padding: 14px; }"
            "QLabel#statTitle { color: #2563eb; font-size: 12px; font-weight: 600; }"
            "QLabel#statValue { color: #0f172a; font-size: 22px; font-weight: 700; padding-top: 4px; }"
        );
    }
}


void MainWindow::onEditRuleClicked()
{
    const int row = m_rulesTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("Edit Rule"), QStringLiteral("Select a rule to edit first."));
        return;
    }

    
    const QVector<Rule> &rules = m_firewallManager.rules();
    if (row >= rules.size())
        return;

    const Rule &selectedRule = rules.at(row);

   
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Edit Rule"));

    auto *form = new QFormLayout(&dialog);

    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setText(selectedRule.name);

    auto *directionCombo = new QComboBox(&dialog);
    directionCombo->addItems({QStringLiteral("Inbound"), QStringLiteral("Outbound")});
    directionCombo->setCurrentIndex(selectedRule.direction == Direction::Inbound ? 0 : 1);

    auto *protocolCombo = new QComboBox(&dialog);
    protocolCombo->addItems({QStringLiteral("TCP"), QStringLiteral("UDP")});
    protocolCombo->setCurrentIndex(selectedRule.protocol == Protocol::Tcp ? 0 : 1);

    auto *portSpin = new QSpinBox(&dialog);
    portSpin->setRange(1, kMaxPort);
    portSpin->setValue(selectedRule.port); 

    auto *actionCombo = new QComboBox(&dialog);
    actionCombo->addItems({QStringLiteral("Accept"), QStringLiteral("Drop")});
    actionCombo->setCurrentIndex(selectedRule.action == Action::Accept ? 0 : 1);

    form->addRow(QStringLiteral("Rule Name:"), nameEdit);
    form->addRow(QStringLiteral("Direction:"), directionCombo);
    form->addRow(QStringLiteral("Protocol:"), protocolCombo);
    form->addRow(QStringLiteral("Port:"), portSpin);
    form->addRow(QStringLiteral("Action:"), actionCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

   
    Rule updatedRule;
    updatedRule.name = nameEdit->text().trimmed();
    updatedRule.direction = directionCombo->currentIndex() == 0 ? Direction::Inbound : Direction::Outbound;
    updatedRule.protocol = protocolCombo->currentIndex() == 0 ? Protocol::Tcp : Protocol::Udp;
    updatedRule.port = portSpin->value();
    updatedRule.action = actionCombo->currentIndex() == 0 ? Action::Accept : Action::Drop;
    updatedRule.enabled = selectedRule.enabled; 
 
    m_firewallManager.removeRule(row);
    m_firewallManager.addRule(updatedRule);
    
    onRefreshClicked();
}
namespace {
QPixmap drawTrafficPieChart(int allowed, int blocked, int size,bool dark)
{
      QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const int total = allowed + blocked;
    const int margin = 8;
    const QRectF outerRect(margin, margin, size - margin * 2, size - margin * 2);

    if (total == 0) {
        painter.setPen(QPen(QColor("#94a3b8"), 14));
        painter.drawArc(outerRect, 0, 360 * 16);
        painter.setPen(QColor("#94a3b8"));
        painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("No data"));
        return pixmap;
    }

    const int allowedSpan = static_cast<int>(360.0 * 16 * allowed / total);
    const int blockedSpan = 360 * 16 - allowedSpan;
    const int penWidth = static_cast<int>(size * 0.16);

    QPen allowedPen(QColor("#22c55e"), penWidth);
    allowedPen.setCapStyle(Qt::FlatCap);
    painter.setPen(allowedPen);
    painter.drawArc(outerRect.adjusted(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0),
                     90 * 16, allowedSpan);

    QPen blockedPen(QColor("#ef4444"), penWidth);
    blockedPen.setCapStyle(Qt::FlatCap);
    painter.setPen(blockedPen);
    painter.drawArc(outerRect.adjusted(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0),
                     90 * 16 + allowedSpan, blockedSpan);

    const int allowedPercent = static_cast<int>(100.0 * allowed / total + 0.5);

    painter.setPen(dark ? QColor("#f8fafc") : QColor("#1e293b"));
    QFont font = painter.font();
    font.setPointSize(size / 9);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("%1%").arg(allowedPercent));

    return pixmap;
}
}

void MainWindow::updateTrafficPieChart()
{
     
       if (!m_trafficPieChart)
        return;

    const bool dark = m_themeCombo && m_themeCombo->currentText() == QStringLiteral("Dark");
    m_trafficPieChart->setPixmap(drawTrafficPieChart(m_allowedPacketCount, m_blockedPacketCount,
                                                       m_trafficPieChart->width(), dark));

    const int total = m_allowedPacketCount + m_blockedPacketCount;
    const int allowedPercent = total > 0 ? static_cast<int>(100.0 * m_allowedPacketCount / total + 0.5) : 0;
    const int blockedPercent = total > 0 ? 100 - allowedPercent : 0;

    if (m_allowedBar)
        m_allowedBar->setValue(allowedPercent);
    if (m_blockedBar)
        m_blockedBar->setValue(blockedPercent);
    }
void MainWindow::detectPortScans()
{
    if (!m_portScanAlertLabel)
        return;

    constexpr int kPortScanThreshold = 5;

    QHash<QString, QSet<QString>> sourcePorts;

    for (int row = 0; row < m_monitorTable->rowCount(); ++row) {
        const QString type = m_monitorTable->item(row, 1)->text();
          const QString rowDirection = m_monitorTable->item(row, 2)->text();

        if (type != QStringLiteral("Allow") && type != QStringLiteral("Block"))
            continue;
        if (rowDirection != QStringLiteral("In"))
            continue;

        const QString source = m_monitorTable->item(row, 4)->text();
        const QString destination = m_monitorTable->item(row, 5)->text();

        const int sourceColonIndex = source.lastIndexOf(QChar(':'));
        const QString sourceIp = sourceColonIndex > 0 ? source.left(sourceColonIndex) : source;

        const int destColonIndex = destination.lastIndexOf(QChar(':'));
        const QString destPort = destColonIndex > 0 ? destination.mid(destColonIndex + 1) : QString();

        if (sourceIp.isEmpty() || destPort.isEmpty())
            continue;

        sourcePorts[sourceIp].insert(destPort);
    }

    QStringList alerts;
    bool blockedSomeone = false;

    for (auto it = sourcePorts.constBegin(); it != sourcePorts.constEnd(); ++it) {
        if (it.value().size() < kPortScanThreshold)
            continue;

        alerts << QStringLiteral("%1 (%2 port)").arg(it.key()).arg(it.value().size());

        if (m_firewallManager.blockIp(it.key()))
            blockedSomeone = true;
    }

    m_portScanAlertLabel->setText(alerts.isEmpty()
        ? QString()
        : QStringLiteral("🚫The IP address was automatically blocked. ") + alerts.join(QStringLiteral(", ")));

    if (blockedSomeone)
        refreshBlockedIpsList();}
        void MainWindow::refreshBlockedIpsList()
{
    if (!m_blockedIpsList)
        return;

    m_blockedIpsList->clear();
    m_blockedIpsList->addItems(m_firewallManager.blockedIps());
}

void MainWindow::onUnblockIpClicked()
{
    QListWidgetItem *item = m_blockedIpsList->currentItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("Unblock IP"), QStringLiteral("Select an IP first."));
        return;
    }

    m_firewallManager.unblockIp(item->text());
    refreshBlockedIpsList();
}
void MainWindow::onManualBlockIpClicked()
{
    const QString ip = m_manualIpInput->text().trimmed();
    if (ip.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Block IP"),
            QStringLiteral("Bir IP adresi veya IP bloğu (CIDR) yaz."));
        return;
    }

    if (!m_firewallManager.blockIp(ip)) {
        QMessageBox::warning(this, QStringLiteral("Block IP"),
            QStringLiteral("Bu adres bloklanamadı — zaten bloklu, korumalı (kendi IP'n/loopback) ya da geçersiz olabilir."));
        return;
    }

    m_manualIpInput->clear();
    refreshBlockedIpsList();
}
#include "mainwindow.h"
#include <QApplication>
#include <QFont>
#include <QSpacerItem>
#include <QCoreApplication>
#include <thread>

// ── Colour constants ─────────────────────────────────────────
static const char* C_BG       = "#0D0D1A";
static const char* C_SIDEBAR  = "#111128";
static const char* C_CARD     = "#16163A";
static const char* C_INPUT_BG = "#13132B";
static const char* C_USER_BUB = "#2D1B69";
static const char* C_BOT_BUB  = "#1A1A35";
static const char* C_ACCENT   = "#7C3AED";
static const char* C_ACLT     = "#A855F7";
static const char* C_HOVER    = "#6D28D9";
static const char* C_TEXT     = "#E8E8FF";
static const char* C_MUTED    = "#6060A0";
static const char* C_BORDER   = "#2D2D5E";
static const char* C_GREEN    = "#4ADE80";
static const char* C_RED      = "#F87171";

// ============================================================
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
    logger("voxmind_log.txt"),
    voiceThread(nullptr),
    voiceWorker(nullptr),
    micActive(false)
{
    setWindowTitle("VoxMind — Voice Assistant  |  OOP Project 2026");
    setMinimumSize(1050, 680);
    resize(1260, 780);

    buildUI();
    applyTheme();
    fillSidebar();

    // ── Setup voice input thread ──────────────────────────────
    bool voiceInputOk = voiceIn.initialize();

    voiceThread = new QThread(this);
    voiceWorker = new VoiceWorker(&voiceIn);
    voiceWorker->moveToThread(voiceThread);

    connect(this,        &MainWindow::startListening,
            voiceWorker, &VoiceWorker::doListen);
    connect(voiceWorker, &VoiceWorker::finished,
            this,        &MainWindow::onVoiceResult);
    connect(voiceThread, &QThread::finished,
            voiceWorker, &QObject::deleteLater);

    voiceThread->start();

    // ── Status label ─────────────────────────────────────────
    if(voiceOut.isReady() && voiceInputOk)
        statusLbl->setText("  ● Voice IN + OUT ready");
    else if(voiceOut.isReady())
        statusLbl->setText("  ● Voice OUT ready  |  Click 🎤 to use mic");
    else
        statusLbl->setText("  ● Run as Administrator for voice");

    // FIXED: Never disable mic button — always allow clicking
    // It will retry initialize() when clicked
    micBtn->setEnabled(true);

    // ── Intro message + VOICE greeting ───────────────────────
    QString intro =
        "Hello! I am VoxMind, your personal voice assistant. "
        "How can I help you today? "
        "Click any shortcut on the left, or type a command below. "
        "Say H or type help to see all available commands!";

    // Show in chat
    addBubble(intro, false);
    logger.log("VoxMind", intro.toStdString());

    // Speak the greeting — FIXED: use QTimer so window shows first
    QTimer::singleShot(600, this, [this, intro](){
        voiceOut.speakAsync(intro.toStdString());
    });
}

MainWindow::~MainWindow() {
    if(voiceThread){
        voiceThread->quit();
        voiceThread->wait(2000);
    }
}

// ============================================================
//  BUILD UI
// ============================================================
void MainWindow::buildUI() {

    central = new QWidget(this);
    setCentralWidget(central);
    rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0,0,0,0);
    rootLayout->setSpacing(0);

    // ── SIDEBAR ──────────────────────────────────────────────
    sidebar = new QFrame(central);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(245);

    QVBoxLayout* sbLayout = new QVBoxLayout(sidebar);
    sbLayout->setContentsMargins(0,0,0,0);
    sbLayout->setSpacing(0);

    // Logo block
    QFrame* logoBlock = new QFrame(sidebar);
    logoBlock->setObjectName("logoBlock");
    QVBoxLayout* logoLay = new QVBoxLayout(logoBlock);
    logoLay->setContentsMargins(16,22,16,18);
    logoLay->setSpacing(4);
    logoLay->setAlignment(Qt::AlignHCenter);

    robotLabel = new QLabel("🤖", logoBlock);
    robotLabel->setObjectName("robotLabel");
    robotLabel->setAlignment(Qt::AlignCenter);

    appName = new QLabel("VoxMind", logoBlock);
    appName->setObjectName("appName");
    appName->setAlignment(Qt::AlignCenter);

    QLabel* tagline = new QLabel("Voice Assistant  •  OOP 2026", logoBlock);
    tagline->setObjectName("tagline");
    tagline->setAlignment(Qt::AlignCenter);

    logoLay->addWidget(robotLabel);
    logoLay->addWidget(appName);
    logoLay->addWidget(tagline);

    // Divider
    auto mkDiv = [&](QWidget* parent){
        QFrame* d = new QFrame(parent);
        d->setObjectName("divider");
        d->setFixedHeight(1);
        return d;
    };

    // Commands header
    QLabel* cmdHdr = new QLabel("  QUICK COMMANDS", sidebar);
    cmdHdr->setObjectName("secHeader");
    cmdHdr->setFixedHeight(34);

    // Command list
    cmdList = new QListWidget(sidebar);
    cmdList->setObjectName("cmdList");
    cmdList->setFrameShape(QFrame::NoFrame);
    cmdList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Status
    statusLbl = new QLabel("  ● Initializing...", sidebar);
    statusLbl->setObjectName("statusLbl");
    statusLbl->setFixedHeight(38);
    statusLbl->setWordWrap(true);

    sbLayout->addWidget(logoBlock);
    sbLayout->addWidget(mkDiv(sidebar));
    sbLayout->addWidget(cmdHdr);
    sbLayout->addWidget(cmdList, 1);
    sbLayout->addWidget(mkDiv(sidebar));
    sbLayout->addWidget(statusLbl);

    // ── CHAT FRAME ───────────────────────────────────────────
    chatFrame = new QFrame(central);
    chatFrame->setObjectName("chatFrame");
    QVBoxLayout* cfLay = new QVBoxLayout(chatFrame);
    cfLay->setContentsMargins(0,0,0,0);
    cfLay->setSpacing(0);

    // Title bar
    QFrame* titleBar = new QFrame(chatFrame);
    titleBar->setObjectName("titleBar");
    titleBar->setFixedHeight(54);
    QHBoxLayout* tbLay = new QHBoxLayout(titleBar);
    tbLay->setContentsMargins(20,0,20,0);

    QLabel* titleLbl = new QLabel("🤖  VoxMind", titleBar);
    titleLbl->setObjectName("titleLbl");

    QLabel* subLbl = new QLabel(
        "Your C++ Voice Assistant  |  OOP Project 2026  |  Team of 3",
        titleBar);
    subLbl->setObjectName("subLbl");

    tbLay->addWidget(titleLbl);
    tbLay->addStretch();
    tbLay->addWidget(subLbl);

    // Scroll area
    scroll = new QScrollArea(chatFrame);
    scroll->setObjectName("scroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    chatBox = new QWidget();
    chatBox->setObjectName("chatBox");
    chatLayout = new QVBoxLayout(chatBox);
    chatLayout->setContentsMargins(24,18,24,18);
    chatLayout->setSpacing(14);
    chatLayout->addStretch();

    scroll->setWidget(chatBox);

    // Input bar
    inputFrame = new QFrame(chatFrame);
    inputFrame->setObjectName("inputFrame");
    inputFrame->setFixedHeight(70);
    QHBoxLayout* inLay = new QHBoxLayout(inputFrame);
    inLay->setContentsMargins(18,12,18,12);
    inLay->setSpacing(10);

    micBtn = new QPushButton("🎤", inputFrame);
    micBtn->setObjectName("micBtn");
    micBtn->setFixedSize(44,44);
    micBtn->setToolTip("Click to listen (Windows Speech Recognition required)");

    inputField = new QLineEdit(inputFrame);
    inputField->setObjectName("inputField");
    inputField->setPlaceholderText(
        "Type a command or shortcode  "
        "(e.g.  O = YouTube   G = Google   joke   time   battery   H = help)...");
    inputField->setFixedHeight(44);

    sendBtn = new QPushButton("➤", inputFrame);
    sendBtn->setObjectName("sendBtn");
    sendBtn->setFixedSize(44,44);

    inLay->addWidget(micBtn);
    inLay->addWidget(inputField, 1);
    inLay->addWidget(sendBtn);

    cfLay->addWidget(titleBar);
    cfLay->addWidget(scroll, 1);
    cfLay->addWidget(inputFrame);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(chatFrame, 1);

    // ── CONNECTIONS ──────────────────────────────────────────
    connect(sendBtn,  &QPushButton::clicked,   this, &MainWindow::onSendClicked);
    connect(inputField,&QLineEdit::returnPressed,this,&MainWindow::onInputReturn);
    connect(cmdList,  &QListWidget::itemClicked,this, &MainWindow::onSidebarClicked);
    connect(micBtn,   &QPushButton::clicked,   this, &MainWindow::onMicClicked);
}

// ============================================================
//  THEME
// ============================================================
void MainWindow::applyTheme() {
    QString ss = QString(R"(
QMainWindow,QWidget { background:%1; color:%2;
    font-family:'Segoe UI',Arial,sans-serif; font-size:13px; }

/* Sidebar */
#sidebar   { background:%3; border-right:1px solid %4; }
#logoBlock { background:%3; }
#robotLabel{ font-size:54px; padding-top:2px; background:transparent; }
#appName   { font-size:22px; font-weight:700; color:%5;
             letter-spacing:2px; background:transparent; }
#tagline   { font-size:10px; color:%6; background:transparent; }
#divider   { background:%4; }
#secHeader { font-size:10px; color:%6; font-weight:700;
             letter-spacing:1.5px; background:%3;
             padding-left:14px; }
#statusLbl { font-size:10px; color:%7; background:%3;
             padding-left:14px; }

/* Command list */
#cmdList { background:%3; border:none; outline:none; }
#cmdList::item {
    color:%8; padding:7px 14px;
    border-left:3px solid transparent; font-size:12px; }
#cmdList::item:hover {
    background:#191940; border-left:3px solid %9; color:%2; }
#cmdList::item:selected {
    background:#1E1A4A; border-left:3px solid %5; color:%2; }

/* Chat frame */
#chatFrame { background:%1; }
#titleBar  { background:%3; border-bottom:1px solid %4; }
#titleLbl  { font-size:16px; font-weight:700; color:%5; }
#subLbl    { font-size:10px; color:%6; }

/* Scroll */
#scroll,#chatBox { background:%1; border:none; }
QScrollBar:vertical { background:%1; width:5px; margin:0; }
QScrollBar::handle:vertical { background:%4; border-radius:3px; min-height:24px; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }

/* Input bar */
#inputFrame { background:%10; border-top:1px solid %4; }
#inputField {
    background:#1A1A40; border:1px solid %4;
    border-radius:22px; color:%2;
    padding:0 18px; font-size:13px; }
#inputField:focus { border:1px solid %9; }

#sendBtn {
    background:%5; border:none; border-radius:22px;
    color:white; font-size:17px; font-weight:700; }
#sendBtn:hover   { background:%11; }
#sendBtn:pressed { background:#5B21B6; }

#micBtn {
    background:#1A1A40; border:1px solid %4;
    border-radius:22px; color:%2; font-size:18px; }
#micBtn:hover   { background:#20204A; border-color:%9; }
#micBtn:pressed { background:%5; }
#micBtn:disabled{ color:%6; border-color:%4; }

/* Mic active state */
#micBtnActive {
    background:%5; border:2px solid %7;
    border-radius:22px; color:white; font-size:18px; }
)")
                     .arg(C_BG)      // 1
                     .arg(C_TEXT)    // 2
                     .arg(C_SIDEBAR) // 3
                     .arg(C_BORDER)  // 4
                     .arg(C_ACLT)    // 5
                     .arg(C_MUTED)   // 6
                     .arg(C_GREEN)   // 7
                     .arg(C_ACLT)    // 8
                     .arg(C_ACCENT)  // 9
                     .arg(C_INPUT_BG)// 10
                     .arg(C_HOVER);  // 11

    setStyleSheet(ss);
}

// ============================================================
//  FILL SIDEBAR
// ============================================================
void MainWindow::fillSidebar() {
    struct Item { QString label; QString cmd; };
    QVector<Item> items = {
                           // ── Websites ──
                           {"🌐  YouTube",       "O"},
                           {"🔍  Google",        "G"},
                           {"📧  Gmail",         "GM"},
                           {"💬  WhatsApp",      "WA"},
                           {"📸  Instagram",     "IG"},
                           {"🐦  Twitter",       "W"},
                           {"📘  Facebook",      "FB"},
                           {"🐙  GitHub",        "GH"},
                           {"🎬  Netflix",       "NF"},
                           {"🛒  Amazon",        "AM"},
                           {"🗺️  Google Maps",   "MAP"},
                           {"🌤️  Weather",       "ACC"},
                           // ── Apps ──
                           {"🧮  Calculator",    "CA"},
                           {"📝  Notepad",       "NP"},
                           {"🎨  Paint",         "PT"},
                           {"📁  Explorer",      "EX"},
                           {"📄  Word",          "D"},
                           {"📊  Excel",         "FT"},
                           {"📑  PowerPoint",    "M"},
                           // ── Folders ──
                           {"💾  Local Disk C",  "DC"},
                           {"💾  Local Disk D",  "DD"},
                           {"⬇️  Downloads",     "DW"},
                           {"🖥️  Desktop",       "DS"},
                           {"📂  Documents",     "DO"},
                           // ── System ──
                           {"🔋  Battery",       "B"},
                           {"📷  Screenshot",    "A"},
                           {"🔒  Lock",          "F"},
                           {"🔇  Mute",          "VM"},
                           {"🔊  Volume Up",     "VU"},
                           {"🔉  Volume Down",   "VD"},
                           {"😴  Sleep",         "nine"},
                           {"🔁  Restart",       "eight"},
                           {"⏹️  Shutdown",      "seven"},
                           {"↩️  Cancel Shut",   "ten"},
                           // ── Fun ──
                           {"😂  Tell a Joke",   "joke"},
                           {"🧠  Fun Fact",      "fact"},
                           {"🪙  Flip Coin",     "flip"},
                           {"🎲  Roll Dice",     "dice"},
                           {"⏰  Time & Date",   "time"},
                           {"❓  Help",          "H"},
                           {"🚪  Exit",          "exit"},
                           };

    for(auto& it : items){
        QListWidgetItem* li = new QListWidgetItem(it.label, cmdList);
        li->setData(Qt::UserRole, it.cmd);
    }
}

// ============================================================
//  ADD CHAT BUBBLE
// ============================================================
void MainWindow::addBubble(const QString& text, bool isUser) {

    // Remove trailing stretch
    chatLayout->takeAt(chatLayout->count()-1);

    QWidget* row = new QWidget(chatBox);
    QHBoxLayout* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0,0,0,0);
    rowLay->setSpacing(8);

    QFrame* bubble = new QFrame(row);
    bubble->setMaximumWidth(isUser ? 540 : 660);

    QVBoxLayout* bLay = new QVBoxLayout(bubble);
    bLay->setContentsMargins(14,10,14,10);
    bLay->setSpacing(4);

    QLabel* tag = new QLabel(isUser ? "You" : "🤖 VoxMind", bubble);
    tag->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; background:transparent;")
                           .arg(isUser ? C_MUTED : C_ACCENT));

    QLabel* msg = new QLabel(text, bubble);
    msg->setWordWrap(true);
    msg->setTextInteractionFlags(Qt::TextSelectableByMouse);
    msg->setStyleSheet(QString("color:%1; font-size:13px; background:transparent;").arg(C_TEXT));

    bLay->addWidget(tag);
    bLay->addWidget(msg);

    bubble->setStyleSheet(
        isUser
            ? QString("background:%1; border-radius:16px 16px 4px 16px;").arg(C_USER_BUB)
            : QString("background:%1; border-radius:16px 16px 16px 4px; border:1px solid %2;")
                  .arg(C_BOT_BUB).arg(C_BORDER));

    if(isUser){
        rowLay->addStretch();
        rowLay->addWidget(bubble);
    } else {
        rowLay->addWidget(bubble);
        rowLay->addStretch();
    }

    chatLayout->addWidget(row);
    chatLayout->addStretch();

    scrollBottom();
}

// ============================================================
//  PROCESS INPUT
// ============================================================
void MainWindow::processInput(const QString& raw) {
    QString txt = raw.trimmed();
    if(txt.isEmpty()) return;

    addBubble(txt, true);
    logger.log("You", txt.toStdString());

    // ── Language mode switch — checked BEFORE command parsing,
    // so "switch to urdu" is never mistaken for an unknown command.
    if (vmCheckLanguageSwitch(txt.toStdString(), currentLanguage)) {
        QString resp = (currentLanguage == VoiceLanguage::Urdu)
            ? "Switched to Urdu mode. Ab main Urdu commands samjhoon gi."
            : "Switched to English mode.";
        addBubble(resp, false);
        logger.log("VoxMind", resp.toStdString());
        voiceOut.speakAsync(resp.toStdString());
        statusLbl->setText(currentLanguage == VoiceLanguage::Urdu
            ? "  ● Language: Urdu"
            : "  ● Language: English");
        return;
    }

    Command* cmd = parser.parse(txt.toStdString());

    if(!cmd){
        QString resp = "I did not understand that. Say H or type help to see all commands.";
        addBubble(resp, false);
        logger.log("VoxMind", resp.toStdString());
        voiceOut.speakAsync(resp.toStdString());
        return;
    }

    // ── Commands that might do real work (recursively searching a
    // drive, etc.) run on a background thread, so the UI keeps
    // responding immediately instead of freezing until they finish.
    if (cmd->isAsync()) {
        addBubble("Searching your PC, one moment...", false);
        std::thread([this, cmd]() {
            std::string result = cmd->execute();
            QMetaObject::invokeMethod(this, [this, result, cmd]() {
                QString resp = QString::fromStdString(result);
                addBubble(resp, false);
                logger.log("VoxMind", result);
                voiceOut.speakAsync(result);
                delete cmd;
            }, Qt::QueuedConnection);
        }).detach();
        return;
    }

    QString resp = QString::fromStdString(cmd->execute());
    bool isExit = (dynamic_cast<ExitCommand*>(cmd) != nullptr);
    delete cmd;

    addBubble(resp, false);
    logger.log("VoxMind", resp.toStdString());

    // Speak response
    voiceOut.speakAsync(resp.toStdString());

    if(isExit){
        QTimer::singleShot(2500, qApp, &QApplication::quit);
    }
}

// ============================================================
//  SLOTS
// ============================================================
void MainWindow::onSendClicked() {
    QString txt = inputField->text().trimmed();
    inputField->clear();
    processInput(txt);
}

void MainWindow::onInputReturn() {
    onSendClicked();
}

void MainWindow::onSidebarClicked(QListWidgetItem* item) {
    QString cmd = item->data(Qt::UserRole).toString();
    if(!cmd.isEmpty()) processInput(cmd);
}

void MainWindow::onMicClicked() {
    if(micActive) return; // already listening

    // Try to initialize voice input if not ready yet
    // This handles case where WSR was started AFTER VoxMind launched
    if(!voiceIn.isReady()) {
        statusLbl->setText("  ● Connecting to microphone...");
        bool ok = voiceIn.initialize();
        if(ok) {
            statusLbl->setText("  ● Voice IN + OUT ready");
        }
    }

    if(voiceIn.isReady()) {
        // SAPI voice input is ready — use it
        setMicState(true);
        addBubble("🎤 Listening... speak your command clearly now!", false);
        voiceOut.speakAsync("Listening");
        emit startListening();
    } else {
        // Fallback: launch Windows Voice Typing (Win+H)
        // Works without WSR — built into Windows 11
        addBubble(
            "🎤 Opening Windows Voice Typing...\n"
            "1. A microphone bar will appear\n"
            "2. Speak your command clearly\n"
            "3. Your words will appear in the text box below\n"
            "4. Press Enter to send!", false);

        // Simulate Win+H keypress to open Voice Typing
        INPUT inputs[4] = {};

        // Press Win
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_LWIN;

        // Press H
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 'H';

        // Release H
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'H';
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

        // Release Win
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_LWIN;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

        // Focus input field first so voice typing types into it
        inputField->setFocus();

        // Small delay then send Win+H
        QTimer::singleShot(300, this, [this, inputs]() mutable {
            SendInput(4, inputs, sizeof(INPUT));
        });
    }
}

void MainWindow::onVoiceResult(const QString& text) {
    setMicState(false);

    if(text.isEmpty()){
        addBubble("🎤 Nothing heard. Please try again.", false);
    } else {
        processInput(text);
    }
}

// ============================================================
//  HELPERS
// ============================================================
void MainWindow::scrollBottom() {
    QTimer::singleShot(60, this, [this](){
        scroll->verticalScrollBar()->setValue(
            scroll->verticalScrollBar()->maximum());
    });
}

void MainWindow::setMicState(bool active) {
    micActive = active;
    if(active){
        micBtn->setText("⏹");
        micBtn->setStyleSheet(
            QString("background:%1; border:2px solid %2;"
                    "border-radius:22px; color:white; font-size:18px;")
                .arg(C_ACCENT).arg(C_GREEN));
        micBtn->setToolTip("Listening...");
    } else {
        micBtn->setText("🎤");
        micBtn->setStyleSheet("");
        micBtn->setToolTip("Click to listen");
    }
}
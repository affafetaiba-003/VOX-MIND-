#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QFrame>
#include <QTimer>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QScrollBar>
#include <QSizePolicy>

#include "voxmind_engine.h"

// ============================================================
//  VoiceWorker — runs listen() on a background thread
//  so the GUI never freezes while waiting for speech
// ============================================================
class VoiceWorker : public QObject {
    Q_OBJECT
public:
    explicit VoiceWorker(VoiceInputHandler* h, QObject* parent=nullptr)
        : QObject(parent), handler(h) {}

public slots:
    void doListen() {
        if(!handler || !handler->isReady()){
            emit finished("");
            return;
        }
        string result = handler->listen(8000);
        emit finished(QString::fromStdString(result));
    }

signals:
    void finished(const QString& text);

private:
    VoiceInputHandler* handler;
};

// ============================================================
//  MainWindow
// ============================================================
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onSendClicked();
    void onInputReturn();
    void onSidebarClicked(QListWidgetItem* item);
    void onMicClicked();
    void onVoiceResult(const QString& text);

signals:
    void startListening();

private:
    // Engine
    CommandParser      parser;
    VoiceOutputHandler voiceOut;
    VoiceInputHandler  voiceIn;
    Logger             logger;

    // Voice thread
    QThread*      voiceThread;
    VoiceWorker*  voiceWorker;
    bool          micActive;

    // Voice language mode — English or Urdu, switched explicitly
    // by saying/typing "switch to urdu" / "switch to english".
    // Kept separate from English/Urdu mixed in one sentence on
    // purpose (see the VoxMind upgrade guide for why).
    VoiceLanguage currentLanguage = VoiceLanguage::English;

    // UI
    QWidget*      central;
    QHBoxLayout*  rootLayout;

    // Sidebar
    QFrame*       sidebar;
    QLabel*       robotLabel;
    QLabel*       appName;
    QListWidget*  cmdList;
    QLabel*       statusLbl;

    // Chat
    QFrame*       chatFrame;
    QScrollArea*  scroll;
    QWidget*      chatBox;
    QVBoxLayout*  chatLayout;

    // Input
    QFrame*       inputFrame;
    QLineEdit*    inputField;
    QPushButton*  sendBtn;
    QPushButton*  micBtn;

    // Helpers
    void buildUI();
    void applyTheme();
    void fillSidebar();
    void addBubble(const QString& text, bool isUser);
    void processInput(const QString& raw);
    void scrollBottom();
    void setMicState(bool active);
};

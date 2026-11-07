// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_overlay.h"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPainterPath>

#include "common/nx2_test_window.h"
#include "main_window.h"

namespace {

constexpr int TickMs = 250;
constexpr double DoneShownSeconds = 60.0;

QString Clock(double seconds) {
    const int whole = static_cast<int>(std::ceil(std::max(seconds, 0.0)));
    return QStringLiteral("%1:%2").arg(whole / 60).arg(whole % 60, 2, 10, QLatin1Char('0'));
}

} // Anonymous namespace

TestOverlay::TestOverlay(MainWindow* parent) : QWidget(parent), m_mainWindow{parent} {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
    setFixedSize(340, 112);
    connect(&m_timer, &QTimer::timeout, this, &TestOverlay::Tick);
    connect(parent, &MainWindow::positionChanged, this, [this](const QPoint&) { Place(); });
    m_timer.start(TickMs);
    Place();
}

bool TestOverlay::IsWanted() {
    return Nx2TestWindow::Get().phase != Nx2TestWindow::Phase::None;
}

void TestOverlay::Place() {
    const QRect frame = m_mainWindow->geometry();
    move(frame.x() + frame.width() - width() - 24, frame.y() + 72);
}

void TestOverlay::Tick() {
    const auto status = Nx2TestWindow::Get();
    if (status.phase == Nx2TestWindow::Phase::None) {
        hide();
        return;
    }
    if (status.phase == Nx2TestWindow::Phase::Done) {
        m_doneFor += TickMs / 1000.0;
        if (m_doneFor > DoneShownSeconds) {
            hide();
            m_timer.stop();
            return;
        }
    }
    Place();
    if (!isVisible()) {
        show();
    }
    update();
}

void TestOverlay::paintEvent(QPaintEvent*) {
    const auto status = Nx2TestWindow::Get();
    QColor accent;
    QString head;
    QString line;
    QString count;
    double progress = 0.0;
    switch (status.phase) {
    case Nx2TestWindow::Phase::Waiting:
        accent = QColor(240, 170, 40);
        head = tr("TEST LICHII EN COURS");
        line = QString::fromStdString(status.prepare);
        count = tr("Enregistrement dans %1").arg(Clock(status.seconds_left));
        break;
    case Nx2TestWindow::Phase::Recording:
        accent = QColor(230, 60, 60);
        head = tr("● ENREGISTREMENT");
        line = QString::fromStdString(status.label);
        count = tr("Encore %1").arg(Clock(status.seconds_left));
        progress = status.seconds_total > 0.0
                       ? 1.0 - status.seconds_left / status.seconds_total
                       : 0.0;
        break;
    case Nx2TestWindow::Phase::Done:
        accent = QColor(70, 190, 100);
        head = tr("✓ TEST TERMINÉ");
        line = tr("Tu peux dire « fini » à Claude");
        count = QString{};
        progress = 1.0;
        break;
    default:
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF box = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(box, 10, 10);
    painter.fillPath(path, QColor(20, 20, 24, 215));
    painter.setPen(QPen(accent, 2));
    painter.drawPath(path);

    QFont font = painter.font();
    font.setPixelSize(13);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(accent);
    painter.drawText(QRectF(16, 10, width() - 32, 20), Qt::AlignLeft | Qt::AlignVCenter, head);

    font.setPixelSize(15);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(16, 34, width() - 32, 24), Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(line, Qt::ElideRight, width() - 32));

    font.setPixelSize(22);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRectF(16, 60, width() - 32, 30), Qt::AlignLeft | Qt::AlignVCenter, count);

    const QRectF bar(16, height() - 16, width() - 32, 5);
    painter.fillRect(bar, QColor(255, 255, 255, 40));
    painter.fillRect(QRectF(bar.x(), bar.y(), bar.width() * std::clamp(progress, 0.0, 1.0),
                            bar.height()),
                     accent);
}

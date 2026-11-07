// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QTimer>
#include <QWidget>

class MainWindow;

/// A small panel at the top right of the window while a timed play test runs
/// (Nx2TestWindow): what to do now, and a countdown to the recording and to its end.
class TestOverlay : public QWidget {
    Q_OBJECT

public:
    explicit TestOverlay(MainWindow* parent);

    /// True when the loaded game has a test window to show.
    static bool IsWanted();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void Tick();
    void Place();

    MainWindow* m_mainWindow = nullptr;
    QTimer m_timer;
    double m_doneFor = 0.0; ///< Seconds the "done" message has been shown.
};

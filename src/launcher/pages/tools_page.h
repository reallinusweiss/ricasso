#pragma once
// ============================================================================
// tools_page.h — External tools and developer shortcuts
// ============================================================================

#include <QWidget>

namespace swordfare::launcher {

class ToolsPage : public QWidget {
    Q_OBJECT

public:
    explicit ToolsPage(QWidget* parent = nullptr);

private slots:
    void on_launch_ruby_clicked();
    void on_open_data_clicked();
    void on_open_saves_clicked();

private:
    void setup_ui();
};

} // namespace swordfare::launcher

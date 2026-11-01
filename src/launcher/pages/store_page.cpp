// ============================================================================
// store_page.cpp — Community Mod Catalog & Download Browser
// ============================================================================

#include "launcher/pages/store_page.h"
#include "launcher/launcher_theme.h"
#include "platform/mod_catalog_embedded.h"
#include "platform/data_path.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QPainter>
#include <QPainterPath>
#include <QTemporaryFile>
#include <set>

namespace swordfare::launcher {

static const char* k_live_store_url =
    "https://raw.githubusercontent.com/raijinswordigo/requests/refs/heads/main/store.json";

StorePage::StorePage(QWidget* parent) : QWidget(parent) {
    m_net_mgr = new QNetworkAccessManager(this);

    // Create mod cache icons dir
    QString cache_icons_dir = QString::fromStdString(get_user_data_dir()) + "/mod_cache/icons";
    QDir().mkpath(cache_icons_dir);

    setup_ui();
    refresh_catalog();
}

void StorePage::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(28, 22, 28, 22);
    main_layout->setSpacing(16);

    // ── Search & Filter Bar ──────────────────────────────────────────────────
    auto* search_row = new QHBoxLayout();
    search_row->setSpacing(12);

    m_edit_search = new QLineEdit(this);
    m_edit_search->setPlaceholderText("Search community mods, packs, overhauls...");
    m_edit_search->addAction(LauncherTheme::fa_icon(ICON_FA_MAGNIFYING_GLASS, QColor(139, 148, 158), 14), QLineEdit::LeadingPosition);
    connect(m_edit_search, &QLineEdit::textChanged, this, &StorePage::on_search_changed);
    search_row->addWidget(m_edit_search, 3);

    m_combo_cat = new QComboBox(this);
    m_combo_cat->addItem("All Categories");
    connect(m_combo_cat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StorePage::on_category_changed);
    search_row->addWidget(m_combo_cat, 1);

    m_btn_refresh = new QPushButton("  Refresh", this);
    m_btn_refresh->setIcon(LauncherTheme::fa_icon(ICON_FA_ARROWS_ROTATE, QColor(160, 174, 192), 13));
    m_btn_refresh->setProperty("class", "SecondaryBtn");
    connect(m_btn_refresh, &QPushButton::clicked, this, &StorePage::refresh_catalog);
    search_row->addWidget(m_btn_refresh);

    main_layout->addLayout(search_row);

    // ── Main Content Split ───────────────────────────────────────────────────
    auto* content_layout = new QHBoxLayout();
    content_layout->setSpacing(18);

    m_list = new QListWidget(this);
    m_list->setSpacing(6);
    m_list->setIconSize(QSize(44, 44));
    connect(m_list, &QListWidget::currentRowChanged, this, &StorePage::on_mod_selected);
    content_layout->addWidget(m_list, 3);

    // Right Card: Mod Details
    auto* card = new QFrame(this);
    card->setObjectName("ModStoreDetailCard");
    card->setStyleSheet(
        "#ModStoreDetailCard {"
        "  background-color: #121824;"
        "  border: 1px solid #1F2737;"
        "  border-radius: 12px;"
        "}"
    );
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(22, 20, 22, 20);
    card_layout->setSpacing(14);

    // Top Header with Icon, Title, and Badges
    auto* top_row = new QHBoxLayout();
    top_row->setSpacing(16);

    m_lbl_detail_icon = new QLabel(card);
    m_lbl_detail_icon->setFixedSize(64, 64);
    top_row->addWidget(m_lbl_detail_icon);

    auto* meta_col = new QVBoxLayout();
    meta_col->setSpacing(4);

    m_lbl_title = new QLabel("Select a Mod", card);
    m_lbl_title->setStyleSheet("font-size: 18px; font-weight: 800; color: #FFFFFF;");
    meta_col->addWidget(m_lbl_title);

    m_lbl_author_cat = new QLabel("Author: —", card);
    m_lbl_author_cat->setStyleSheet("font-size: 12px; color: #8B949E; font-weight: 500;");
    meta_col->addWidget(m_lbl_author_cat);

    m_lbl_status_badge = new QLabel(card);
    m_lbl_status_badge->setStyleSheet("background-color: #162E1C; color: #3FB950; border: 1px solid #244C2E; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    meta_col->addWidget(m_lbl_status_badge);

    top_row->addLayout(meta_col, 1);
    card_layout->addLayout(top_row);

    // Separator
    auto* sep = new QFrame(card);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background-color: #1F2737;");
    card_layout->addWidget(sep);

    auto* desc_title = new QLabel("OVERVIEW & DETAILS", card);
    desc_title->setStyleSheet("font-size: 11px; font-weight: 700; color: #6E7681; letter-spacing: 0.8px;");
    card_layout->addWidget(desc_title);

    // Markdown description box
    m_txt_desc = new QTextBrowser(card);
    m_txt_desc->setOpenExternalLinks(true);
    m_txt_desc->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #0A0D14;"
        "  border: 1px solid #1C2433;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "  color: #E6EDF3;"
        "  font-size: 13px;"
        "  line-height: 1.45;"
        "}"
    );
    card_layout->addWidget(m_txt_desc, 1);

    // Progress Bar (hidden unless downloading)
    m_progress_bar = new QProgressBar(card);
    m_progress_bar->setFixedHeight(14);
    m_progress_bar->setVisible(false);
    card_layout->addWidget(m_progress_bar);

    m_btn_install = new QPushButton("  Download && Install", card);
    m_btn_install->setIcon(LauncherTheme::fa_icon(ICON_FA_DOWNLOAD, Qt::white, 14));
    m_btn_install->setStyleSheet(
        "QPushButton {"
        "  background-color: #E94560; color: white; border: none; font-weight: bold; border-radius: 6px;"
        "  padding: 10px 18px; font-size: 14px;"
        "}"
        "QPushButton:hover { background-color: #FF5B79; }"
        "QPushButton:pressed { background-color: #D63447; }"
        "QPushButton:disabled { background-color: #242D3E; color: #6E7681; }"
    );
    m_btn_install->setEnabled(false);
    m_btn_install->setCursor(Qt::PointingHandCursor);
    connect(m_btn_install, &QPushButton::clicked, this, &StorePage::on_install_clicked);
    card_layout->addWidget(m_btn_install);

    content_layout->addWidget(card, 2);
    main_layout->addLayout(content_layout);
}

void StorePage::refresh_catalog() {
    m_btn_refresh->setEnabled(false);

    // 1. Try to load cached catalog first for instant UI response
    QString cache_file = QString::fromStdString(get_user_data_dir()) + "/store_cache.json";
    if (QFile::exists(cache_file)) {
        QFile f(cache_file);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.readAll();
            load_catalog_data(data.toStdString(), false);
        }
    } else {
        // Fallback to embedded catalog
        std::string json(reinterpret_cast<const char*>(modman::k_demo_catalog_json), modman::k_demo_catalog_json_len);
        load_catalog_data(json, false);
    }

    // 2. Fetch fresh catalog from GitHub asynchronously
    QUrl url(k_live_store_url);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net_mgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        on_catalog_downloaded(reply);
    });
}

void StorePage::on_catalog_downloaded(QNetworkReply* reply) {
    m_btn_refresh->setEnabled(true);
    reply->deleteLater();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        if (!data.isEmpty()) {
            // Save to cache
            QString cache_file = QString::fromStdString(get_user_data_dir()) + "/store_cache.json";
            QFile f(cache_file);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(data);
            }
            load_catalog_data(data.toStdString(), true);
        }
    }
}

void StorePage::load_catalog_data(const std::string& json, bool) {
    m_all_mods = modman::parse_catalog(json);
    m_installed_mask = modman::catalog_installed_mask(m_all_mods, get_user_data_dir() + "/mods");

    std::set<std::string> cats;
    for (const auto& m : m_all_mods) {
        if (!m.category.empty()) cats.insert(m.category);
    }

    QString cur_cat = m_combo_cat->currentText();
    m_combo_cat->blockSignals(true);
    m_combo_cat->clear();
    m_combo_cat->addItem("All Categories");
    for (const auto& c : cats) {
        m_combo_cat->addItem(QString::fromStdString(c));
    }
    int idx = m_combo_cat->findText(cur_cat);
    if (idx >= 0) m_combo_cat->setCurrentIndex(idx);
    m_combo_cat->blockSignals(false);

    // Download remote icons in background
    for (const auto& m : m_all_mods) {
        if (!m.icon_url.empty()) {
            download_remote_icon(QString::fromStdString(m.id), QString::fromStdString(m.icon_url));
        }
    }

    filter_list();
}

void StorePage::download_remote_icon(const QString& mod_id, const QString& url_str) {
    QString cache_path = QString::fromStdString(get_user_data_dir()) + "/mod_cache/icons/" + mod_id + ".png";
    if (QFile::exists(cache_path)) return;

    if (m_pending_icon_downloads.count(mod_id)) return;
    m_pending_icon_downloads.insert(mod_id);

    QUrl url(url_str);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net_mgr->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, mod_id, cache_path]() {
        reply->deleteLater();
        m_pending_icon_downloads.erase(mod_id);
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            if (!data.isEmpty()) {
                QFile f(cache_path);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(data);
                }
                on_icon_downloaded(mod_id, data);
            }
        }
    });
}

void StorePage::on_icon_downloaded(const QString& mod_id, const QByteArray&) {
    // Update any matching list item
    for (int i = 0; i < m_list->count(); ++i) {
        auto* item = m_list->item(i);
        int mod_idx = item->data(Qt::UserRole).toInt();
        if (mod_idx >= 0 && mod_idx < static_cast<int>(m_all_mods.size())) {
            if (QString::fromStdString(m_all_mods[mod_idx].id) == mod_id) {
                item->setIcon(QIcon(get_cached_or_default_icon(m_all_mods[mod_idx], 44)));
                break;
            }
        }
    }

    // Update detail card if currently displayed
    int row = m_list->currentRow();
    if (row >= 0 && row < static_cast<int>(m_filtered_indices.size())) {
        int idx = m_filtered_indices[row];
        if (QString::fromStdString(m_all_mods[idx].id) == mod_id) {
            m_lbl_detail_icon->setPixmap(get_cached_or_default_icon(m_all_mods[idx], 64));
        }
    }
}

QPixmap StorePage::get_cached_or_default_icon(const modman::StoreMod& m, int size) {
    QPixmap raw;
    QString cache_icon = QString::fromStdString(get_user_data_dir()) + "/mod_cache/icons/" + QString::fromStdString(m.id) + ".png";
    if (QFile::exists(cache_icon)) {
        raw.load(cache_icon);
    }
    // Try installed folder icon
    if (raw.isNull()) {
        QString inst_icon = QString::fromStdString(get_user_data_dir()) + "/mods/" + QString::fromStdString(m.id) + "/icon.png";
        if (QFile::exists(inst_icon)) raw.load(inst_icon);
    }
    // Fallback procedural / FA icon
    if (raw.isNull()) {
        raw = LauncherTheme::fa_pixmap(ICON_FA_PUZZLE_PIECE, QColor(233, 69, 96), size);
    }

    const int scale = 2;
    const int px = size * scale;
    QPixmap out(px, px);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addRoundedRect(0, 0, px, px, 12, 12);
    p.setClipPath(path);
    p.drawPixmap(0, 0, raw.scaled(px, px, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    p.end();

    out.setDevicePixelRatio(scale);
    return out;
}

void StorePage::filter_list() {
    m_list->clear();
    m_filtered_indices.clear();

    QString query = m_edit_search->text().trimmed().toLower();
    QString cat = m_combo_cat->currentText();

    for (size_t i = 0; i < m_all_mods.size(); ++i) {
        const auto& m = m_all_mods[i];
        QString name = QString::fromStdString(m.name);
        QString desc = QString::fromStdString(m.description);
        QString mcat = QString::fromStdString(m.category);

        if (cat != "All Categories" && mcat != cat) continue;
        if (!query.isEmpty() && !name.toLower().contains(query) && !desc.toLower().contains(query)) continue;

        m_filtered_indices.push_back(static_cast<int>(i));

        bool installed = (i < m_installed_mask.size()) && m_installed_mask[i];
        QString status_tag = installed ? "  [Installed]" : "";

        QString text = QString("%1  (v%2)%3\nCategory: %4  ·  Author: %5\n%6")
                           .arg(name)
                           .arg(QString::fromStdString(m.version.empty() ? "1.0" : m.version))
                           .arg(status_tag)
                           .arg(mcat.isEmpty() ? "General" : mcat)
                           .arg(QString::fromStdString(m.author.empty() ? "Community" : m.author))
                           .arg(desc.left(85) + (desc.length() > 85 ? "..." : ""));

        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, static_cast<int>(i));
        item->setIcon(QIcon(get_cached_or_default_icon(m, 44)));
        m_list->addItem(item);
    }

    if (!m_filtered_indices.empty()) {
        m_list->setCurrentRow(0);
    } else {
        m_lbl_detail_icon->clear();
        m_lbl_title->setText("No Mods Found");
        m_lbl_author_cat->setText("");
        m_lbl_status_badge->setVisible(false);
        m_txt_desc->setPlainText("No community mods matched your search criteria.");
        m_btn_install->setEnabled(false);
    }
}

void StorePage::on_search_changed(const QString&) {
    filter_list();
}

void StorePage::on_category_changed(int) {
    filter_list();
}

void StorePage::on_mod_selected(int row) {
    if (row < 0 || row >= static_cast<int>(m_filtered_indices.size())) return;
    int idx = m_filtered_indices[row];
    const auto& m = m_all_mods[idx];

    m_lbl_detail_icon->setPixmap(get_cached_or_default_icon(m, 64));
    m_lbl_title->setText(QString::fromStdString(m.name));
    m_lbl_author_cat->setText(QString("Author: %1  |  Category: %2  |  v%3")
                                  .arg(QString::fromStdString(m.author.empty() ? "Community" : m.author))
                                  .arg(QString::fromStdString(m.category.empty() ? "General" : m.category))
                                  .arg(QString::fromStdString(m.version.empty() ? "1.0" : m.version)));

    bool installed = (idx < static_cast<int>(m_installed_mask.size())) && m_installed_mask[idx];
    m_lbl_status_badge->setVisible(true);
    if (installed) {
        m_lbl_status_badge->setText("INSTALLED");
        m_lbl_status_badge->setStyleSheet("background-color: #162E1C; color: #3FB950; border: 1px solid #244C2E; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
        m_btn_install->setText("  Reinstall Mod");
    } else {
        m_lbl_status_badge->setText("AVAILABLE FOR DOWNLOAD");
        m_lbl_status_badge->setStyleSheet("background-color: #1D2A44; color: #58A6FF; border: 1px solid #2B3D66; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
        m_btn_install->setText("  Download && Install");
    }

    // Markdown description parsing
    QString desc = QString::fromStdString(m.long_description.empty() ? m.description : m.long_description);
    desc.replace("\\n", "\n");
    m_txt_desc->setMarkdown(desc);

    m_btn_install->setEnabled(!m.download_url.empty());
}

void StorePage::on_install_clicked() {
    int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_filtered_indices.size())) return;
    int idx = m_filtered_indices[row];
    const auto& m = m_all_mods[idx];

    if (m.download_url.empty()) {
        QMessageBox::warning(this, "Installation Unavailable", "This mod does not have a direct download URL.");
        return;
    }

    m_btn_install->setEnabled(false);
    m_progress_bar->setVisible(true);
    m_progress_bar->setRange(0, 100);
    m_progress_bar->setValue(0);

    QUrl url(QString::fromStdString(m.download_url));
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net_mgr->get(req);

    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 done, qint64 total) {
        if (total > 0) {
            m_progress_bar->setValue(static_cast<int>((done * 100) / total));
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, m, idx]() {
        m_progress_bar->setVisible(false);
        m_btn_install->setEnabled(true);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, "Download Failed", QString("Could not download mod package:\n%1").arg(reply->errorString()));
            return;
        }

        QByteArray data = reply->readAll();
        if (data.isEmpty()) {
            QMessageBox::warning(this, "Download Failed", "Downloaded package is empty.");
            return;
        }

        // Save to temp zip file
        QTemporaryFile temp_zip;
        if (!temp_zip.open()) {
            QMessageBox::warning(this, "Install Failed", "Could not create temporary archive on disk.");
            return;
        }
        temp_zip.write(data);
        temp_zip.flush();

        // Install mod archive
        std::string mods_dir = get_user_data_dir() + "/mods";
        modman::ModMeta out_meta;
        std::string err;
        if (modman::install_mod_zip(temp_zip.fileName().toStdString(), mods_dir, &out_meta, &err)) {
            QMessageBox::information(this, "Mod Installed", QString("Successfully installed \"%1\" (v%2)!").arg(QString::fromStdString(m.name)).arg(QString::fromStdString(m.version)));
            m_installed_mask = modman::catalog_installed_mask(m_all_mods, mods_dir);
            filter_list();
            emit modInstalled();
        } else {
            QMessageBox::warning(this, "Extraction Failed", QString("Could not extract mod package:\n%1").arg(QString::fromStdString(err)));
        }
    });
}

} // namespace swordfare::launcher

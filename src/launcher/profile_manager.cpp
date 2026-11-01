// ============================================================================
// profile_manager.cpp — User profile and game save stats implementation
// ============================================================================

#include "launcher/profile_manager.h"
#include "launcher/launcher_theme.h"
#include "platform/save_editor.h"
#include "platform/data_path.h"
#include "platform/launcher_config.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <iostream>

namespace swordfare::launcher {

ProfileManager& ProfileManager::instance() {
    static ProfileManager s_instance;
    return s_instance;
}

ProfileManager::ProfileManager() {
    load();
    refresh_game_stats();
}

QString ProfileManager::profile_file_path() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.local/share/swordigo-desktop";
    QDir().mkpath(base);
    return base + "/launcher_profile.json";
}

QString ProfileManager::avatars_dir_path() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.local/share/swordigo-desktop";
    QString av_dir = base + "/avatars";
    QDir().mkpath(av_dir);
    return av_dir;
}

void ProfileManager::load() {
    QString path = profile_file_path();
    QFile file(path);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            m_profile.username = obj.value("username").toString("Hero");
            m_profile.avatar_path = obj.value("avatar_path").toString();
            m_profile.avatar_preset = obj.value("avatar_preset").toInt(0);
            m_profile.bio = obj.value("bio").toString("Wandering swordsman in the world of Swordigo.");
            m_profile.favorite_skin = obj.value("favorite_skin").toInt(0);
            m_profile.total_playtime_minutes = obj.value("playtime_minutes").toInt(0);
            m_profile.join_date = obj.value("join_date").toString("September 2026");
            m_profile.is_online = obj.value("is_online").toBool(false);
            m_profile.auth_token = obj.value("auth_token").toString();
        }
    } else {
        // Also sync from existing launcher.toml if present
        LauncherConfig lc = launcher_config_load();
        if (!lc.profile_name.empty()) {
            m_profile.username = QString::fromStdString(lc.profile_name);
        }
        if (!lc.profile_avatar.empty()) {
            m_profile.avatar_path = QString::fromStdString(lc.profile_avatar);
        }
        m_profile.join_date = QDateTime::currentDateTime().toString("MMMM yyyy");
        save();
    }
}

void ProfileManager::save() {
    QString path = profile_file_path();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        obj["username"] = m_profile.username;
        obj["avatar_path"] = m_profile.avatar_path;
        obj["avatar_preset"] = m_profile.avatar_preset;
        obj["bio"] = m_profile.bio;
        obj["favorite_skin"] = m_profile.favorite_skin;
        obj["playtime_minutes"] = m_profile.total_playtime_minutes;
        obj["join_date"] = m_profile.join_date;
        obj["is_online"] = m_profile.is_online;
        obj["auth_token"] = m_profile.auth_token;

        QJsonDocument doc(obj);
        file.write(doc.toJson());
    }

    // Sync back to launcher.toml for engine compatibility
    LauncherConfig lc = launcher_config_load();
    lc.profile_name = m_profile.username.toStdString();
    lc.profile_avatar = m_profile.avatar_path.toStdString();
    launcher_config_save(lc);
}

void ProfileManager::set_username(const QString& name) {
    if (name.trimmed().isEmpty()) return;
    m_profile.username = name.trimmed();
    save();
    emit profileChanged();
}

void ProfileManager::set_bio(const QString& bio) {
    m_profile.bio = bio;
    save();
    emit profileChanged();
}

void ProfileManager::set_favorite_skin(int skin_id) {
    m_profile.favorite_skin = skin_id;
    save();
    emit profileChanged();
}

void ProfileManager::set_avatar_preset(int preset_id) {
    m_profile.avatar_preset = preset_id;
    save();
    emit profileChanged();
}

bool ProfileManager::import_custom_avatar(const QString& source_file_path) {
    QFile src(source_file_path);
    if (!src.exists()) return false;

    QImage img(source_file_path);
    if (img.isNull()) return false;

    QString dest_path = avatars_dir_path() + "/user_avatar.png";
    if (img.save(dest_path, "PNG")) {
        m_profile.avatar_path = dest_path;
        m_profile.avatar_preset = 0; // 0 = custom avatar
        save();
        emit profileChanged();
        return true;
    }
    return false;
}

QPixmap ProfileManager::get_preset_pixmap(int preset_id, int diameter) {
    switch (preset_id) {
        case 1: return LauncherTheme::make_preset_avatar("H", diameter, QColor("#E94560")); // Hiro (Red)
        case 2: return LauncherTheme::make_preset_avatar("K", diameter, QColor("#3B82F6")); // Knight (Blue)
        case 3: return LauncherTheme::make_preset_avatar("S", diameter, QColor("#8B5CF6")); // Shadow (Purple)
        case 4: return LauncherTheme::make_preset_avatar("M", diameter, QColor("#10B981")); // Mage (Emerald)
        case 5: return LauncherTheme::make_preset_avatar("G", diameter, QColor("#F59E0B")); // Golden (Gold)
        default: return LauncherTheme::make_preset_avatar(m_profile.username, diameter, QColor("#E94560"));
    }
}

QPixmap ProfileManager::get_avatar_pixmap(int diameter) {
    if (m_profile.avatar_preset > 0) {
        return get_preset_pixmap(m_profile.avatar_preset, diameter);
    }
    if (!m_profile.avatar_path.isEmpty() && QFile::exists(m_profile.avatar_path)) {
        QPixmap raw(m_profile.avatar_path);
        if (!raw.isNull()) {
            return LauncherTheme::make_circular_avatar(raw, diameter);
        }
    }
    return get_preset_pixmap(1, diameter);
}

void ProfileManager::refresh_game_stats() {
    m_stats = LiveGameStats();
    std::string save_dir = get_vfs_save_dir();
    auto paths = save_list_dir(save_dir);

    if (!paths.empty()) {
        SaveFile sf;
        if (save_load(paths[0], sf)) {
            m_stats.coins = sf.game_state.character.coins;
            m_stats.health = sf.game_state.character.health;
            m_stats.level = sf.game_state.character.level;
            m_stats.xp = sf.game_state.character.xp;
            m_stats.percent_completed = sf.percent_completed;
            m_stats.save_name = QString::fromStdString(sf.name);
            m_stats.has_save = true;
        }
    }
    emit statsUpdated();
}

} // namespace swordfare::launcher

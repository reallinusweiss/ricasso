#pragma once
// ============================================================================
// profile_manager.h — Real persistent user profile & avatar management
// ============================================================================

#include <QString>
#include <QPixmap>
#include <QObject>
#include <vector>

namespace swordfare::launcher {

struct LiveGameStats {
    int     coins = 0;
    int     health = 0;
    int     level = 0;
    int     xp = 0;
    float   percent_completed = 0.0f;
    QString save_name;
    bool    has_save = false;
};

struct UserProfile {
    QString username = "Hero";
    QString avatar_path;
    int     avatar_preset = 0; // 0=custom, 1=Hiro, 2=Knight, 3=Corrupt Hero, 4=Mage, 5=Golden
    QString bio = "Wandering swordsman in the world of Swordigo.";
    int     favorite_skin = 0;
    int     total_playtime_minutes = 0;
    QString join_date = "September 2026";
    bool    is_online = false; // future server sync flag
    QString auth_token;
};

class ProfileManager : public QObject {
    Q_OBJECT

public:
    static ProfileManager& instance();

    const UserProfile& profile() const { return m_profile; }
    const LiveGameStats& stats() const { return m_stats; }

    void set_username(const QString& name);
    void set_bio(const QString& bio);
    void set_favorite_skin(int skin_id);
    void set_avatar_preset(int preset_id);
    bool import_custom_avatar(const QString& source_file_path);

    QPixmap get_avatar_pixmap(int diameter = 80);
    QPixmap get_preset_pixmap(int preset_id, int diameter = 80);

    void load();
    void save();
    void refresh_game_stats();

signals:
    void profileChanged();
    void statsUpdated();

private:
    ProfileManager();
    QString profile_file_path() const;
    QString avatars_dir_path() const;

    UserProfile   m_profile;
    LiveGameStats m_stats;
};

} // namespace swordfare::launcher

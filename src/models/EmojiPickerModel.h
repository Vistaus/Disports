#pragma once

#include <QAbstractListModel>
#include <QVariantList>

#include <vector>

// Emoji shown in the emoji picker: the current server's custom emoji, or
// one category of Unicode emoji, or search results across both.
class EmojiPickerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)
    Q_PROPERTY(bool hasServerEmoji READ hasServerEmoji NOTIFY serverEmojiChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        IsCustomRole = Qt::UserRole + 1,
        TextRole,       // Unicode emoji
        ImageUrlRole,   // custom emoji
        NameRole,
        ReactionRole,   // what to react with: the emoji, or "name:id"
        InsertTextRole, // what to put in the composer: the emoji, or ":name:"
                        // (the core's ResolveMentions turns it into <:name:id>)
    };

    explicit EmojiPickerModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString category() const { return m_category; }
    void setCategory(const QString& category);
    QString search() const { return m_search; }
    void setSearch(const QString& search);
    QVariantList categories() const;
    bool hasServerEmoji() const { return !m_serverEmoji.empty(); }

    // Re-reads the current server's custom emoji.
    void reloadServerEmoji();

signals:
    void categoryChanged();
    void searchChanged();
    void serverEmojiChanged();
    void countChanged();

private:
    struct Entry {
        bool custom = false;
        QString text;      // Unicode emoji
        QString name;      // label or custom emoji name
        QString category;
        quint64 id = 0;    // custom emoji
        bool animated = false;
    };

    void loadUnicode();
    void rebuild();

    std::vector<Entry> m_unicode;
    std::vector<Entry> m_serverEmoji;
    std::vector<const Entry*> m_rows;
    QString m_category = QStringLiteral("faces");
    QString m_search;
};

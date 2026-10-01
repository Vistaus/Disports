#!/bin/bash
# translations.sh update
#     Collects the texts to translate (i18n.tr() in qml/, tr() in src/)
#     into po/disports.jukfiuu.pot, and brings every po/<lang>.po up to
#     date with it. The template is only rewritten when the texts changed,
#     so running it twice changes nothing.
# translations.sh add <lang>
#     Starts po/<lang>.po (for example de, nl, pt_BR) for a new language.
#
# The build compiles each po/<lang>.po on its own (see CMakeLists.txt);
# nothing here runs during the build. Needs gettext.
set -eu
ROOT=$(readlink -f "$(dirname "$0")/..")
PO=$ROOT/po
POT=$PO/disports.jukfiuu.pot
cd "$ROOT"

update() {
    mkdir -p "$PO"
    local new
    new=$(mktemp --suffix=.pot)
    trap 'rm -f "$new"' RETURN
    # tr("a") or tr("a", "as", n) in QML; tr("a") in C++. The client core
    # (src/discord) has no texts of its own to show.
    local common=(--from-code=UTF-8 --add-comments=TRANSLATORS --sort-by-file --add-location=file
                  --package-name=Disports --copyright-holder="Martin Filipov"
                  --msgid-bugs-address=https://github.com/jukfiuune/Disports/issues)
    xgettext -o "$new" "${common[@]}" -L JavaScript --keyword=tr:1,1t --keyword=tr:1,2,3t \
        $(find qml -name '*.qml' | sort)
    xgettext -j -o "$new" "${common[@]}" -L C++ --keyword=tr:1 \
        $(find src -path src/discord -prune -o -name '*.cpp' -print | sort)
    sed -i 's/charset=CHARSET/charset=UTF-8/' "$new"

    # Only the creation date differs: keep the old file.
    if [ -f "$POT" ] && diff -q <(grep -v '^"POT-Creation-Date' "$POT") \
                               <(grep -v '^"POT-Creation-Date' "$new") > /dev/null; then
        echo "texts unchanged"
    else
        cp "$new" "$POT"
        echo "updated $(basename "$POT"): $(grep -c '^msgid ' "$POT") texts"
    fi
    for po in "$PO"/*.po; do
        [ -e "$po" ] || continue
        msgmerge --update --backup=none --quiet "$po" "$POT"
        # Texts the app no longer has.
        msgattrib --no-obsolete --output-file="$po" "$po"
        echo "$(basename "$po" .po): $(msgfmt --statistics -o /dev/null "$po" 2>&1)"
    done
}

add() {
    local lang=$1
    [ -f "$POT" ] || update
    if [ -e "$PO/$lang.po" ]; then
        echo "po/$lang.po already exists"
        exit 1
    fi
    msginit --no-translator --locale="$lang" --input="$POT" --output-file="$PO/$lang.po"
    echo "translate the msgstr lines of po/$lang.po, then build"
}

case "${1:-}" in
    update) update ;;
    add) [ $# -eq 2 ] || { echo "usage: $0 add <lang>"; exit 1; }; add "$2" ;;
    *) sed -n '2,12s/^# \{0,1\}//p' "$0"; exit 1 ;;
esac

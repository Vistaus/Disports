# Translating Disports

Each language is one file, `po/<language>.po`, holding every text in the app. The texts to translate are collected in the template, `po/disports.jukfiuu.pot`.

## Starting a new language

Use the language's code, such as `de`, `nl` or `pt_BR`:

```bash
tools/translations.sh add de
```

This creates `po/de.po` from the template. It needs gettext, which most Linux systems have.

## Translating

Fill in the `msgstr` line under each `msgid`, either by hand or with an editor such as [Poedit](https://poedit.net):

```
msgid "Log out"
msgstr "Abmelden"
```

- Keep placeholders like `%1` and `%2`. They are replaced by names, numbers and so on.
- Some texts have a singular and a plural (`msgid_plural`). Fill in every `msgstr[n]` line your language needs.
- A text left empty shows in English.

The next build includes the language. The app follows the phone's language set in System Settings.

## When the app's texts change

```bash
tools/translations.sh update
```

This refreshes the template from the code and brings every language file up to date, marking new and changed texts. Nothing changes when the texts are the same, and the build never runs it by itself.

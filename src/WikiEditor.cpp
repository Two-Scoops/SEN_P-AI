#include "WikiEditor.h"
#include <QStringList>
#include <QMap>
#include <algorithm>
#include <QRegularExpression>
#include <QRegExp>

const QString WikiEditor::imageUrlRequestString = QStringLiteral("/w/api.php?action=query&format=json&prop=imageinfo&titles=File%3A[TEAM]+icon.png%7CFile%3A[TEAM]+logo.png&iiprop=url");
const QString WikiEditor::teamInfoRequestString = QStringLiteral("/w/api.php?action=query&format=json&prop=revisions&titles=[TEAM]&redirects=1&rvprop=content&rvlimit=1&rvsection=0");


const QString wikiMatchInProgress = QStringLiteral("<center><u>'''In Progress'''</u></center>\n");
const QString wikiEditorWarning = QStringLiteral("<!--WARNING: this section is being edited by Rigbot. Please wait until this message has been removed to make any changes-->\n");





WikiEditor::WikiEditor()
{

}

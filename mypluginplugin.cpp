/*
 * Copyright (c) 2020 Kay Gawlik <kaydev@amarunet.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "mypluginplugin.h"
#include "mypluginconstants.h"

#include <coreplugin/icore.h>
#include <coreplugin/icontext.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <texteditor/texteditor.h>


#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QUuid>


namespace
{
	void addUUID(Core::IEditor* editor)
	{
		TextEditor::TextEditorWidget* editorWidget = qobject_cast<TextEditor::TextEditorWidget*>(editor->widget());

		QUuid uuid = QUuid::createUuid();

		QString uuidString = uuid.toString(QUuid::WithoutBraces);
		QString uuidIdString = QString(R"(0x%1, 0x%2, 0x%3, {0x%4, 0x%5, 0x%6, 0x%7, 0x%8, 0x%9, 0x%10, 0x%11})")
		        .arg(uuidString.mid( 0, 8))
		        .arg(uuidString.mid( 9, 4))
		        .arg(uuidString.mid(14, 4))
		        .arg(uuidString.mid(19, 2))
		        .arg(uuidString.mid(21, 2))
		        .arg(uuidString.mid(24, 2))
		        .arg(uuidString.mid(26, 2))
		        .arg(uuidString.mid(28, 2))
		        .arg(uuidString.mid(30, 2))
		        .arg(uuidString.mid(32, 2))
		        .arg(uuidString.mid(34, 2));


		QString uuidCodeString = QString(R"(// %1
constexpr const GUID the_uuid = {%2};)").arg(uuidString).arg(uuidIdString);

		editorWidget->insertCodeSnippet(editorWidget->textCursor(), uuidCodeString);
	}


	QStringList alginColumn(const QString& text)
	{

		enum class CommentState { None, Line, Inline };

		int countParenthesis = 0; // (
		int countBracket     = 0; // {

		class Line
		{
		public:
			std::vector<QString> columns;
		};

		std::vector<int> columnLength;
		std::vector<Line> lines;

		Line actLine;
		QString column;
		int lastNonSpaceChar = 0;
		int columnBegin      = 0;
		int actChar          = 0;
		CommentState commentState = CommentState::None;

		QChar lastChar;

		for(QChar c : text)
		{
			++actChar;

			switch(commentState)
			{
				case CommentState::None:
				{
					if(c == '*' && lastChar == '/')
						commentState = CommentState::Inline;
					if(c == '/' && lastChar == '/')
						commentState = CommentState::Line;

					if(c == '{')
						++countBracket;
					if(c == '(')
						++countParenthesis;
					const int countBracketParenthesis = countBracket+countParenthesis;

					if(c == '}')
						--countBracket;
					if(c == ')')
						--countParenthesis;


					const bool isColumnChar = (c == ',' || c == '(' || c == ')' || c == '{' || c == '}');
					if(isColumnChar && countBracketParenthesis < 2)
					{
						int columnLengt = lastNonSpaceChar - columnBegin;
						if(columnLengt == 0)
							continue;

						QString colString = text.mid(columnBegin, lastNonSpaceChar-columnBegin);
						actLine.columns.push_back(std::move(colString));
						columnBegin = actChar-1;

						if(columnLength.size() < actLine.columns.size())
							columnLength.push_back(columnLengt);
						else
						{
							int& cLength = columnLength[actLine.columns.size()-1];
							if(cLength < columnLengt)
								cLength = columnLengt;
						}
						continue;
					}
					break;
				}
			    case CommentState::Line:
				    break;
			    case CommentState::Inline:
				    if(c == '/' && lastChar == '*')
						commentState = CommentState::None;
				    break;
			}

			if(c == ' ')
				continue;

			lastNonSpaceChar = actChar;
			lastChar = c;

			if(c == QChar::ParagraphSeparator || c == QChar::LineSeparator || c == '\n')
			{
				QString colString = text.mid(columnBegin, actChar-columnBegin-1);
				actLine.columns.push_back(std::move(colString));
				columnBegin = actChar;

				lines.push_back(std::move(actLine));
				actLine = Line();

				commentState = CommentState::None;
				lastChar = QChar();
				continue;
			}
		}
		const QString colString = text.mid(columnBegin);
		actLine.columns.push_back(colString);
		lines.push_back(std::move(actLine));

		columnLength.push_back(0);

		QStringList alignText;
		for(const Line& line : lines)
		{
			QString l;
			for(std::size_t i = 0; i < line.columns.size(); ++i)
			{
				const QString& colText = line.columns[i];
				l += colText;
				int delta = columnLength[i] - colText.size();
				if(delta>0)
					l += QString(delta, ' ');
			}
			alignText << l;
		}
		return alignText;
	}

	void alignColumn(Core::IEditor* editor)
	{
		TextEditor::TextEditorWidget* editorWidget = qobject_cast<TextEditor::TextEditorWidget*>(editor->widget());

		const QString     selection    = editorWidget->selectedText();
		const QStringList alginedLines = alginColumn(selection);

		if(editorWidget->hasBlockSelection())
		{
			QTextCursor cursor = editorWidget->textCursor();
			int dummy;
			int anchorRow;
			int posRow;
			editorWidget->convertPosition(cursor.anchor()  , &anchorRow, &dummy);
			editorWidget->convertPosition(cursor.position(), &posRow   , &dummy);

			const int firstCol = editorWidget->verticalBlockSelectionFirstColumn();
			const int lastCol  = editorWidget->verticalBlockSelectionLastColumn();

			const int firstRow = std::min(anchorRow, posRow);
			const int minPos   = std::min(cursor.anchor(), cursor.position());
			const int maxPos   = std::max(cursor.anchor(), cursor.position());

			QString textBlock = editorWidget->textAt(minPos-firstCol, maxPos-minPos+firstCol);

			cursor.clearSelection();
			cursor.beginEditBlock();
			editorWidget->gotoLine(firstRow, 0);
			editorWidget->remove(maxPos-minPos+firstCol);

			QStringList l = textBlock.split('\n');
			if(l.size() == 1)
				l = textBlock.split(QChar::ParagraphSeparator);
			if(l.size() == 1)
				l = textBlock.split(QChar::LineSeparator);

			QStringList text;
			const int num = std::min(l.size(), alginedLines.size());
			for(int i = 0; i < num; ++i)
			{
				const QString& line = l[i];
				const QString& alginBlock = alginedLines[i];

				text << line.mid(0, firstCol) + alginBlock + line.mid(lastCol);
			}
			editorWidget->insertPlainText(text.join('\n'));
			cursor.endEditBlock();

		}
		else
		{
			editorWidget->insertPlainText(alginedLines.join('\n'));
		}
	}
}

namespace MyPlugin {
	namespace Internal {

		MyPluginPlugin::MyPluginPlugin()
		{
			// Create your members
		}

		MyPluginPlugin::~MyPluginPlugin()
		{
			// Unregister objects from the plugin manager's object pool
			// Delete members
		}

		bool MyPluginPlugin::initialize(const QStringList& arguments, QString* errorString)
		{
			// Register objects in the plugin manager's object pool
			// Load settings
			// Add actions to menus
			// Connect to other plugins' signals
			// In the initialize function, a plugin can be sure that the plugins it
			// depends on have initialized their members.

			Q_UNUSED(arguments)
			Q_UNUSED(errorString)


			auto action = new QAction(tr("Align columns"), this);
			connect(action, &QAction::triggered, this, &MyPluginPlugin::alginColumnSlot);
			Core::Command *cmdAlgin = Core::ActionManager::registerAction(action, Constants::ALIGIN_COLUMN_ID, Core::Context(Core::Constants::C_GLOBAL));
			cmdAlgin->setDefaultKeySequence(QKeySequence(tr("Alt+Shift+Z")));

			auto genUUIDaction = new QAction(tr("Generate UUID variable"), this);
			Core::Command *cmd = Core::ActionManager::registerAction(genUUIDaction, Constants::ACTION_ID, Core::Context(Core::Constants::C_GLOBAL));
			connect(genUUIDaction, &QAction::triggered, this, &MyPluginPlugin::addUUIDAction);

			auto activateBlockModeAction = new QAction(tr("Activate Block Mode"), this);
			Core::Command *cmdBlockMode = Core::ActionManager::registerAction(activateBlockModeAction, Constants::ACTIVATE_BLOCK_MODE_ID, Core::Context(Core::Constants::C_GLOBAL));
			connect(activateBlockModeAction, &QAction::triggered, this, &MyPluginPlugin::activateBlockMode);

			Core::ActionContainer *menu = Core::ActionManager::createMenu(Constants::MENU_ID);
			menu->menu()->setTitle(tr("MyPlugin"));
			menu->addAction(cmd);
			menu->addAction(cmdAlgin);
			menu->addAction(cmdBlockMode);
			Core::ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

//			Core::ICore::mainWindow()->menuBar()->addAction(action);
			return true;
		}

		void MyPluginPlugin::extensionsInitialized()
		{
			// Retrieve objects from the plugin manager's object pool
			// In the extensionsInitialized function, a plugin can be sure that all
			// plugins that depend on it are completely initialized.
		}

		ExtensionSystem::IPlugin::ShutdownFlag MyPluginPlugin::aboutToShutdown()
		{
			// Save settings
			// Disconnect from signals that are not needed during shutdown
			// Hide UI (if you add UI that is not in the main window directly)
			return SynchronousShutdown;
		}

		void MyPluginPlugin::alginColumnSlot()
		{

			Core::IEditor* editor = Core::EditorManager::instance()->currentEditor();
			if(!editor)
				return;

			alignColumn(editor);
		}

		void MyPluginPlugin::addUUIDAction()
		{
			Core::IEditor* editor = Core::EditorManager::instance()->currentEditor();
			if(!editor)
				return;

			addUUID(editor);
		}

		void MyPluginPlugin::activateBlockMode()
		{
			Core::IEditor* editor = Core::EditorManager::instance()->currentEditor();
			if(!editor)
				return;
			TextEditor::TextEditorWidget* editorWidget = qobject_cast<TextEditor::TextEditorWidget*>(editor->widget());
			if(editorWidget)
				editorWidget->setBlockSelection(true);
		}

	} // namespace Internal
} // namespace MyPlugin

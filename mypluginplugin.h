#ifndef CREATORPLUGIN_H
#define CREATORPLUGIN_H

#include "myplugin_global.h"

#include <extensionsystem/iplugin.h>

#include<coreplugin/documentmanager.h>


namespace MyPlugin {
	namespace Internal {

		class MyPluginPlugin : public ExtensionSystem::IPlugin
		{
			Q_OBJECT
			Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "MyPlugin.json")

		public:
			MyPluginPlugin();
			~MyPluginPlugin() override;

			bool initialize(const QStringList &arguments, QString *errorString) override;
			void extensionsInitialized() override;
			ShutdownFlag aboutToShutdown() override;

		private slots:
			void alginColumnSlot();
			void addUUIDAction();
			void activateBlockMode();
		};

	} // namespace Internal
} // namespace MyPlugin

#endif // CREATORPLUGIN_H

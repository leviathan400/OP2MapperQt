#pragma once

#include <QString>

class QWidget;

// Launch Outpost 2, launch it into the DLL picker (installing the bundled
// op2launcher.exe + Launcher.dll into <OP2>/OPU first), or open the game
// folder. Port of OP2Mapper3 OP2Launcher.vb.
namespace OP2Launcher {

void launchOutpost2(QWidget *parent);
void launchDllPicker(QWidget *parent);
void openGameFolder(QWidget *parent);

} // namespace OP2Launcher

// Plays <OP2>/OPU/base/sounds/beep9.wav or an embedded sound, asynchronously.
namespace Sounds {
void playFile(const QString &path);
void playResource(const QString &resourcePath);
} // namespace Sounds

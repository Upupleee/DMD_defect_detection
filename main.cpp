#include "mainwindow.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置公司或项目名，让 QSettings 知道存在哪里
    QCoreApplication::setOrganizationName("DMDInspection");
    QCoreApplication::setApplicationName("DMDInspection");

    // 1. 读取保存的语言设置，如果没有，则默认尝试本地系统语言
    QSettings settings;
    QString langCode = settings.value("Language", "auto").toString();

    QTranslator translator;
    bool loaded = false;

    // 2. 根据设置判断我们要加载哪一个语言包
    if (langCode == "auto") {
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = "DMDInspection_" + QLocale(locale).name();
            if (translator.load(":/i18n/" + baseName)) {
                a.installTranslator(&translator);
                loaded = true;
                break;
            }
        }
    } else if (langCode == "zh_CN") {
        if (translator.load(":/i18n/DMDInspection_zh_CN.qm")) {
            a.installTranslator(&translator);
            loaded = true;
        }
    } else if (langCode == "en_US") {
        // 如果想切回英语，Qt 默认基底字符串本来就是英语，不装载 translator 即可。
        // 当然如果你专门有英文包，也可以在这里加载。
    }

    // 3. 构建并显示主窗体
    MainWindow w;
    w.show();
    return a.exec();
}

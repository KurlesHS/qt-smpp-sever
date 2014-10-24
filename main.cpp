#include <QCoreApplication>
#include <QStringList>
#include <QTextCodec>
#include <QProcess>
#include <QDir>


int main(int argc, char *argv[])
{
    QTextCodec *cyrillicCodec = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForTr(cyrillicCodec);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("Win-1251"));
    QTextCodec::setCodecForCStrings(cyrillicCodec);

    QCoreApplication a(argc, argv);
    QStringList arguments = a.arguments();
    QString command = arguments.takeFirst();
    bool terminalMode = false;
    for(const QString &arg : arguments) {
        if (arg == "-t") {
            terminalMode = true;
        }
    }
    if (!terminalMode) {
        QStringList args = {"-t"};
        return QProcess::startDetached(command, args, QDir::currentPath()) ? 0 : -1;
    }

    return a.exec();
}

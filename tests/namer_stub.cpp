// Local subprocess fixture: never invokes Claude or sends a transcript.
#include <QCoreApplication>
#include <QTimer>
#include <cstdio>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const bool quick = qEnvironmentVariable("DEBRIEF_STUB_NAMER_MODE") == "quick";
    QTimer::singleShot(quick ? 30 : 60000, &app, [&] {
        std::puts("quick test title");
        app.quit();
    });
    return app.exec();
}

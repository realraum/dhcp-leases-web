#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

int main(int argc, char *argv[])
{
    QCoreApplication app{argc, argv};

    QCommandLineParser parser;
    parser.setApplicationDescription("dhcpd lease parser");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("leases-file", QCoreApplication::translate("main", "Source dhcpd leases file, usually under /var/lib/dhcp/dhcpd.leases"));
    parser.addPositionalArgument("hosts-file", QCoreApplication::translate("main", "Destination hosts file"));
    parser.addPositionalArgument("table-file", QCoreApplication::translate("main", "Destination table file"));
    parser.process(app);

    if (parser.positionalArguments().size() != 3)
        qFatal("2 arguments needed");

    QFile leasesFile{parser.positionalArguments().at(0)};
    if (!leasesFile.open(QIODevice::ReadOnly | QIODevice::Text))
        qFatal("could not open leases file for reading: %s", qPrintable(leasesFile.errorString()));

    QTextStream leasesStream{&leasesFile};

    QFile hostsFile{parser.positionalArguments().at(1)};
    if (!hostsFile.open(QIODevice::WriteOnly | QIODevice::Text))
        qFatal("could not open hosts file for writing: %s", qPrintable(hostsFile.errorString()));

    QTextStream hostsStream{&hostsFile};

    QFile tableFile{parser.positionalArguments().at(2)};
    if (!tableFile.open(QIODevice::WriteOnly | QIODevice::Text))
        qFatal("could not open table file for writing: %s", qPrintable(tableFile.errorString()));

    QTextStream tableStream{&tableFile};

    enum { StateIdle, StateInLease } state { StateIdle };

    const QRegularExpression ipExpr{"^ *lease +([^ ]+) \{"};
    const QRegularExpression hostnameExpr{"^ +client-hostname \"([^\"]+)\""};
    const QRegularExpression macaddressExpr{"^ +hardware ethernet ([^;]+);"};
    const QRegularExpression startsExpr{"^ +starts [0-9]+ ([^;]+);"};
    const QRegularExpression endsExpr{"^ +ends [0-9]+ ([^;]+);"};

    QString ip;
    QString hostname;
    QString macaddress;
    QString starts;
    QString ends;

    tableStream << "<table border=\"1\">"
                       "<thead>"
                           "<tr>"
                               "<th>IP</th>"
                               "<th>Hostname</th>"
                               "<th>MAC Address</th>"
                               "<th>Starts</th>"
                               "<th>Ends</th>"
                           "</tr>"
                       "</thead>"
                       "<tbody>";

    while (!leasesStream.atEnd())
    {
        auto line = leasesStream.readLine();
        if (const auto match = ipExpr.match(line); match.hasMatch())
        {
            if (state != StateIdle)
                qWarning() << "new lease begin when parser not idle";

            state = StateInLease;

            ip = match.captured(1);
        }
        else if (const auto match = hostnameExpr.match(line); match.hasMatch())
        {
            if (state != StateInLease)
                qWarning() << "hostname when parser idle";

            hostname = match.captured(1);
        }
        else if (const auto match = macaddressExpr.match(line); match.hasMatch())
        {
            if (state != StateInLease)
                qWarning() << "macaddress when parser idle";

            macaddress = match.captured(1);
        }
        else if (const auto match = startsExpr.match(line); match.hasMatch())
        {
            if (state != StateInLease)
                qWarning() << "starts when parser idle";

            starts = match.captured(1);
        }
        else if (const auto match = endsExpr.match(line); match.hasMatch())
        {
            if (state != StateInLease)
                qWarning() << "ends when parser idle";

            ends = match.captured(1);
        }
        else if (line == "}")
        {
            if (state != StateInLease)
                qWarning() << "lease end when parser idle";

            state = StateIdle;

//            qDebug() << ip << hostname << macaddress;

            if (!ip.isEmpty() && !hostname.isEmpty() && !hostname.contains(' '))
            {
                hostsStream << ip << ' ' << hostname << ".local " << hostname << '\n';
            }

            tableStream << "<tr>"
                               "<td>" << ip.toHtmlEscaped() << "</td>"
                               "<td>" << hostname.toHtmlEscaped() << "</td>"
                               "<td>" << macaddress.toHtmlEscaped() << "</td>"
                               "<td>" << starts.toHtmlEscaped() << "</td>"
                               "<td>" << ends.toHtmlEscaped() << "</td>"
                           "</tr>";

            ip.clear();
            hostname.clear();
            macaddress.clear();
            starts.clear();
            ends.clear();
        }
        else
        {
            //qWarning() << "unknown" << line;
        }
    }

    tableStream << "</tbody>"
                   "</table>";

    return 0;
}

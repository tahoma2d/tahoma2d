

#include "tfarmexecutor.h"
#include <QStringList>
//------------------------------------------------------------------------------

TFarmExecutor::TFarmExecutor(int port)
	: TTcpIpServer(port)
{
}

//------------------------------------------------------------------------------

int extractArgs(const QString &s, vector<QString> &argv)
{
	argv.clear();
	if (s == "")
		return 0;

	QStringList sl = s.split(',');
	int i;
	for (i = 0; i < sl.size(); i++)
		argv.push_back(sl.at(i));

	return argv.size();
}

//------------------------------------------------------------------------------

void TFarmExecutor::onReceive(int socket, const QString &data)
{
	QString reply;

	try {
		vector<QString> argv;
		extractArgs(data, argv);
		reply = execute(argv);
	} catch (...) {
	}

	sendReply(socket, reply);
}

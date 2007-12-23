/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "repository.h"
#include <QTextStream>

Repository::Repository(const Rules::Repository &rule)
{
    foreach (Rules::Repository::Branch branchRule, rule.branches) {
        Branch branch;
        branch.branchFrom = branchRule.branchFrom;
        branch.isCreated = false;

        branches.insert(branchRule.name, branch);
    }

    fastImport.setWorkingDirectory(rule.name);
    fastImport.setProcessChannelMode(QProcess::ForwardedChannels);
}

Repository::~Repository()
{
    if (fastImport.state() == QProcess::Running) {
        fastImport.closeWriteChannel();
        fastImport.waitForFinished();
    }
}

Repository::Transaction *Repository::newTransaction(const QString &branch, const QString &svnprefix,
                                                    int revnum)
{
    if (!branches.contains(branch))
        return 0;

    Transaction *txn = new Transaction;
    txn->repository = this;
    txn->branch = branch.toUtf8();
    txn->svnprefix = svnprefix.toUtf8();
    txn->datetime = 0;
    txn->revnum = revnum;
    txn->lastmark = revnum;

    if (fastImport.state() == QProcess::NotRunning) {
        // start the process
#ifndef DRY_RUN
        fastImport.start("git-fast-import", QStringList());
#else
        fastImport.start("/bin/cat", QStringList());
#endif
    }

    return txn;
}

Repository::Transaction::~Transaction()
{
}

void Repository::Transaction::setAuthor(const QByteArray &a)
{
    author = a;
}

void Repository::Transaction::setDateTime(uint dt)
{
    datetime = dt;
}

void Repository::Transaction::setLog(const QByteArray &l)
{
    log = l;
}

void Repository::Transaction::deleteFile(const QString &path)
{
    deletedFiles.append(path);
}

QIODevice *Repository::Transaction::addFile(const QString &path, int mode, qint64 length)
{
    FileProperties fp;
    fp.mode = mode;
    fp.mark = ++lastmark;

#ifndef DRY_RUN
    repository->fastImport.write("blob\nmark :");
    repository->fastImport.write(QByteArray::number(fp.mark));
    repository->fastImport.write("\ndata ");
    repository->fastImport.write(QByteArray::number(length));
    repository->fastImport.write("\n", 1);
    repository->fastImport.waitForBytesWritten(0);
#endif

    modifiedFiles.insert(path, fp);
    return &repository->fastImport;
}

void Repository::Transaction::commit()
{
    // create the commit message
    QByteArray message = log;
    if (!message.endsWith('\n'))
        message += '\n';
    message += "\nsvn=" + svnprefix + "; revision=" + QByteArray::number(revnum) + "\n";

    {
        QByteArray branchRef = branch;
        if (!branchRef.startsWith("refs/heads/"))
            branchRef.prepend("refs/heads/");

        QTextStream s(&repository->fastImport);
        s << "commit " << branchRef << endl;
        s << "mark :" << revnum << endl;
        s << "committer " << author << ' ' << datetime << "-0000" << endl;

        Branch &br = repository->branches[branch];
        if (!br.isCreated) {
            br.isCreated = true;
            s << "from " << br.branchFrom << endl;
        }

        s << "data " << message.length() << endl;
    }

    repository->fastImport.write(message);

    // write the file deletions
    foreach (QString df, deletedFiles)
        repository->fastImport.write("D " + df.toUtf8() + "\n");

    // write the file modifications
    QHash<QString, FileProperties>::ConstIterator it = modifiedFiles.constBegin();
    for ( ; it != modifiedFiles.constEnd(); ++it) {
        repository->fastImport.write("M ", 2);
        repository->fastImport.write(QByteArray::number(it->mode, 8));
        repository->fastImport.write(" :", 2);
        repository->fastImport.write(QByteArray::number(it->mark));
        repository->fastImport.write(" ", 1);
        repository->fastImport.write(it.key().toUtf8());
        repository->fastImport.write("\n", 1);
    }

    repository->fastImport.write("\n");

    while (repository->fastImport.bytesToWrite() && repository->fastImport.waitForBytesWritten()) {
        // nothing
    }
}
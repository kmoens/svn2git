/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
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

#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QDebug>

#include <limits.h>
#include <stdio.h>

#include "CommandLineParser.h"
#include "ruleparser.h"
#include "repository.h"
#include "svn.h"

QHash<QByteArray, QByteArray> loadIdentityMapFile(const QString &fileName)
{
    QHash<QByteArray, QByteArray> result;
    if (fileName.isEmpty())
        return result;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        fprintf(stderr, "Could not open file %s: %s",
                qPrintable(fileName), qPrintable(file.errorString()));
        return result;
    }

    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        int comment_pos = line.indexOf('#');
        if (comment_pos != -1)
            line.truncate(comment_pos);
        line = line.trimmed();
        int space = line.indexOf(' ');
        if (space == -1)
            continue;           // invalid line

        // Support git-svn author files, too
        // - svn2git native:  loginname Joe User <user@example.com>
        // - git-svn:         loginname = Joe User <user@example.com>
        int rightspace = line.indexOf(" = ");
        int leftspace = space;
        if (rightspace == -1) {
            rightspace = space;
        } else {
          leftspace = rightspace;
          rightspace += 2;
        }

        QByteArray realname = line.mid(rightspace).trimmed();
        line.truncate(leftspace);

        result.insert(line, realname);
    };
    file.close();

    return result;
}

static const CommandLineOption options[] = {
    {"--identity-map FILENAME", "provide map between svn username and email"},
    {"--identity-domain DOMAIN", "provide user domain if no map was given"},
    {"--rules FILENAME[,FILENAME]", "the rules file(s) that determines what goes where"},
    {"--msg-filter FILENAME", "External program / script to modify svn log message"},
    {"--add-metadata", "if passed, each git commit will have svn commit info"},
    {"--add-metadata-notes", "if passed, each git commit will have notes with svn commit info"},
    {"--dry-run", "don't actually write anything"},
    {"--create-dump", "don't create the repository but a dump file suitable for piping into fast-import"},
    {"--debug-rules", "print what rule is being used for each file"},
    {"--commit-interval NUMBER", "if passed the cache will be flushed to git every NUMBER of commits"},
    {"--stats", "after a run print some statistics about the rules"},
    {"--svn-branches", "Use the contents of SVN when creating branches, Note: SVN tags are branches as well"},
    {"--empty-dirs", "Add .gitignore-file for empty dirs"},
    {"--svn-ignore", "Import svn-ignore-properties via .gitignore"},
    {"--propcheck", "Check for svn-properties except svn-ignore"},
    {"--fast-import-timeout SECONDS", "number of seconds to wait before terminating fast-import, 0 to wait forever"},
    {"-h, --help", "show help"},
    {"-v, --version", "show version"},
    CommandLineLastOption
};

int main(int argc, char **argv)
{
    printf("Invoked as:'");
    for(int i = 0; i < argc; ++i)
        printf(" %s", argv[i]);
    printf("'\n");
    CommandLineParser::init(argc, argv);
    CommandLineParser::addOptionDefinitions(options);
    Stats::init();
    CommandLineParser *args = CommandLineParser::instance();
    if(args->contains(QLatin1String("version"))) {
        printf("Git version: %s\n", VER);
        return 0;
    }
    if (args->contains(QLatin1String("help")) || args->arguments().count() != 1) {
        args->usage(QString(), "[Path to subversion repo]");
        return 0;
    }
    if (args->undefinedOptions().count()) {
        QTextStream out(stderr);
        out << "svn-all-fast-export failed: ";
        bool first = true;
        foreach (QString option, args->undefinedOptions()) {
            if (!first)
                out << "          : ";
            out << "unrecognized option or missing argument for; `" << option << "'" << endl;
            first = false;
        }
        return 10;
    }
    if (!args->contains("rules")) {
        QTextStream out(stderr);
        out << "svn-all-fast-export failed: please specify the rules using the 'rules' argument\n";
        return 11;
    }
    if (!args->contains("identity-map") && !args->contains("identity-domain")) {
        QTextStream out(stderr);
        out << "WARNING; no identity-map or -domain specified, all commits will use default @localhost email address\n\n";
    }

    QCoreApplication app(argc, argv);
    // Load the configuration
    RulesList rulesList(args->optionArgument(QLatin1String("rules")));
    rulesList.load();

    // create the repository list
    QHash<QString, Repository *> repositories;

    foreach (Rules::Repository rule, rulesList.allRepositories()) {
        Repository *repo = createRepository(rule, repositories);
        if (!repo)
            return EXIT_FAILURE;
        repositories.insert(rule.name, repo);

        //repo->setupIncremental(INT_MAX);
	}

    Svn::initialize();
    Svn svn(args->arguments().first());
    svn.setMatchRules(rulesList.allMatchRules());
    svn.setRepositories(repositories);
    svn.setIdentityMap(loadIdentityMapFile(args->optionArgument("identity-map")));
    // Massage user input a little, no guarantees that input makes sense.
    QString domain = args->optionArgument("identity-domain").simplified().remove(QChar('@'));
    if (domain.isEmpty())
        domain = QString("localhost");
    svn.setIdentityDomain(domain);

    bool errors = svn.exportAll();

    foreach (Repository *repo, repositories) {
        repo->finalizeTags();
        delete repo;
    }
    Stats::instance()->printStats();
    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

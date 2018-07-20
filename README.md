svn-all-fast-export aka svn2git (forked)
========================================
This project contains all the tools required to do a conversion of an svn repository (server side, not a checkout) to one or more git repositories.

This is the tool used to convert KDE's Subversion into multiple Git repositories.  You can find more description and usage examples at https://techbase.kde.org/Projects/MoveToGit/UsingSvn2Git

This fork differs from the original code in such a way that it was optimized for handling the legacy repository structures at Cipal Schaubroeck. It contains the following modifications:
* *New feature*: support to import from multiple SVN repositories into a single Git repository.
* *New feature*: support to ignore a certain revision inside a rule.
* *New feature*: support for simple conditions inside a match rule.
* *New feature*: more advanced detection of branches in copy operations.
* *Changed behavior*: fail-fast in case of problems with branch detections.
* *Removed feature*: incremental scanning of SVN repositories and partial scanning of SVN repositories. This feature was removed since it was hard to keep it in combination with the import from multiple SVN repositories.

How does it work
----------------
The svn2git repository gets you an application that will do the actual conversion.
The conversion exists of looping over each and every commit in the subversion repository and matching the changes to a ruleset after which the changes are applied to a certain path in a git repo.
The ruleset can specify which git repository to use and thus you can have more than one git repository as a result of running the conversion.
Also noteworthy is that you can have a rule that, for example, changes in svnrepo/branches/foo/2.1/ will appear as a git-branch in a repository.

If you have a proper ruleset the tool will create the git repositories for you and show progress while converting commit by commit.

After it is done you likely want to run `git repack -a -d -f` to compress the pack file as it can get quite big.

Building the tool
-----------------
Run `qmake && make`.  You get `./svn-all-fast-export`.
(Do a checkout of the repo .git' and run qmake and make. You can only build it after having installed libsvn-dev, and naturally Qt. Running the command will give you all the options you can pass to the tool.)

You will need to have some packages to compile it. For Ubuntu distros, use this command to install them all:
`sudo apt-get install build-essential subversion git qtchooser qt5-default libapr1 libapr1-dev libsvn-dev`

KDE
---
there is a repository kde-ruleset which has several example files and one file that should become the final ruleset for the whole of KDE called 'kde-rules-main'.

Write the Rules
---------------
You need to write a rules file that describes how to slice the Subversion history into Git repositories and branches. See https://techbase.kde.org/Projects/MoveToGit/UsingSvn2Git.
The rules are also documented in the 'samples' directory of the svn2git repository. Feel free to add more documentation here as well.

Work flow
---------
Please feel free to fill this section in.

Some SVN tricks
---------------
You can access your newly rsynced SVN repo with commands like `svn ls file:///path/to/repo/trunk/KDE`.
A common issue is tracking when an item left playground for kdereview and then went from kdereview to its final destination. There is no straightforward way to do this. So the following command comes in handy: `svn log -v file:///path/to/repo/kde-svn/kde/trunk/kdereview | grep /trunk/kdereview/mplayerthumbs -A 5 -B 5` This will print all commits relevant to the package you are trying to track. You can also pipe the above command to head or tail to see the the first and last commit it was in that directory.

Match Rules
-----------

The following gives an example of a match rule:
```
match /Foo/branches/([^/]+)/Foo/
    repository foo
    prefix Foo/

    branch releases/\1
    min revision 14000
    max revision 24335

    ignore revision 19708
    ignore revision 22038
end match
```

Inside the match, we can include the following instructions:

| Instruction          | Description |
| -----------          | ----------- |
| `action ignore`      | Any file/dir matching this rule should be ignored. |
| `action export`      | Any file/dir matching this rule should be migrated (default). |
| `action recurse`     | ? |
| `action excluded`    | Any file/dir matching this rule should be ignored, but the information is kept for potential copy operations. |
| `if copy`            | The rule only match if the file/dir in question represents an SVN copy action. |
| `repository <name> ` | Any file/dir matching this rule will be exported to the repository with this name. |
| `branch <name>`      | The branch on which the change needs to be placed. |
| `prefix <path>`      | An optional prefix inside the repository to be added to the path. |
| `min revision <rev>` | This rule applies from this revision only (included). |
| `max revision <rev>` | This rule applies until this revision only (included). |
| `ignore revision <rev>` | Despite the min/max rules, this specific revision will be skipped for the rule. May be specified multiple times. |


